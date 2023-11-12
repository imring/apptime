#include "database.hpp"

#include <sqlite3.h>

namespace fs = std::filesystem;

auto parse_time(const std::string &time_str) {
    using namespace std::chrono;

    constexpr const char    *time_format = "%Y-%m-%d %T";
    system_clock::time_point result;
    std::istringstream       ss{time_str};

#if __cpp_lib_chrono >= 201907L // std::chrono::parse
    ss >> parse(time_format, result);
#else
    std::tm timeinfo = {};
    ss >> std::get_time(&timeinfo, time_format);

    // read subseconds
    int subseconds = 0;
    if (ss.peek() == '.') {
        ss.ignore();
        ss >> subseconds;
    }

    result = system_clock::from_time_t(std::mktime(&timeinfo)) + system_clock::duration{subseconds};
#endif

    return result;
}

bool in_same_directory(const fs::path &current, const fs::path &directory) {
    if (current.empty() || directory.empty()) {
        return false;
    }
    if (current.root_name().compare(directory.root_name()) != 0) { // it's used in windows to compare disk names
        return false;
    }
    const fs::path relative = current.lexically_relative(directory);
    return !relative.empty() && relative.begin()->compare("..") != 0;
}

namespace apptime {
database::database(std::string_view path) : db_{path.data(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
    // create the applications table
    db_.exec("CREATE TABLE IF NOT EXISTS applications ("
             "id INTEGER NOT NULL,"
             "path TEXT NOT NULL,"
             "name TEXT NOT NULL,"
             "PRIMARY KEY (id AUTOINCREMENT))");

    // create the active_logs table
    db_.exec("CREATE TABLE IF NOT EXISTS active_logs ("
             "program_id INTEGER NOT NULL,"
             "start TIMESTAMP NOT NULL,"
             "end TIMESTAMP NOT NULL,"
             "PRIMARY KEY (program_id, start),"
             "FOREIGN KEY (program_id) REFERENCES applications(id))");

    // create the focus_logs table
    db_.exec("CREATE TABLE IF NOT EXISTS focus_logs ("
             "program_id INTEGER NOT NULL,"
             "start TIMESTAMP NOT NULL,"
             "end TIMESTAMP NOT NULL,"
             "PRIMARY KEY (program_id, start),"
             "FOREIGN KEY (program_id) REFERENCES applications(id))");

    // create the ignores table
    db_.exec("CREATE TABLE IF NOT EXISTS ignores ("
             "type CHECK(type IN ('file', 'path')) NOT NULL,"
             "value TEXT NOT NULL)");

    // create the ignore function
    db_.createFunction("IS_IGNORED", 1, false, this, &is_ignored_sqlite);
}

void database::add_active(const process &p) {
    if (!valid_application(p)) {
        return;
    }

    const std::string path  = p.full_path();
    const std::string start = std::format("{:L%F %T}", p.start());
    const std::string end   = std::format("{:L%F %T}", std::chrono::system_clock::now());

    SQLite::Statement insert{db_, "INSERT OR REPLACE INTO active_logs (program_id, start, end) VALUES "
                                  "((SELECT id FROM applications WHERE path=?), ?, ?)"};
    insert.bind(1, path);
    insert.bind(2, start);
    insert.bind(3, end);
    insert.exec();
}

void database::add_focus(const process &p) {
    if (!valid_application(p)) {
        return;
    }

    const std::string path  = p.full_path();
    const std::string start = std::format("{:L%F %T}", p.focused_start());
    const std::string end   = std::format("{:L%F %T}", std::chrono::system_clock::now());

    SQLite::Statement insert{db_, "INSERT OR REPLACE INTO focus_logs (program_id, start, end) VALUES "
                                  "((SELECT id FROM applications WHERE path=?), ?, ?)"};
    insert.bind(1, path);
    insert.bind(2, start);
    insert.bind(3, end);
    insert.exec();
}

void database::add_ignore(ignore_type type, std::string_view value) {
    const std::string type_str = type == ignore_file ? "file" : type == ignore_path ? "path" : "";
    if (type_str.empty()) {
        return;
    }

    SQLite::Statement insert{db_, "INSERT INTO ignores(type, value) VALUES (?, ?)"};
    insert.bind(1, type_str);
    insert.bind(2, std::string{value});
    insert.exec();
}

void database::remove_ignore(ignore_type type, std::string_view value) {
    const std::string type_str = type == ignore_file ? "file" : type == ignore_path ? "path" : "";
    if (type_str.empty()) {
        return;
    }

    SQLite::Statement remove{db_, "DELETE FROM ignores WHERE type=? AND value=?"};
    remove.bind(1, type_str);
    remove.bind(2, std::string{value});
    remove.exec();
}

std::vector<active> database::actives() const {
    return actives_detail("active_logs");
}

std::vector<active> database::actives(int year) const {
    return actives_detail("active_logs", std::format(" AND strftime('%Y', logs.start)='{}'", year));
}

std::vector<active> database::actives(int year, int month) const {
    return actives_detail("active_logs", std::format(" AND strftime('%Y-%m', logs.start)='{}-{}'", year, month));
}

std::vector<active> database::actives(int year, int month, int day) const {
    return actives_detail("active_logs", std::format(" AND strftime('%Y-%m-%d', logs.start)='{}-{}-{}'", year, month, day));
}

std::vector<active> database::focuses() const {
    return actives_detail("focus_logs");
}

std::vector<active> database::focuses(int year) const {
    return actives_detail("focus_logs", std::format(" AND strftime('%Y', logs.start)='{}'", year));
}

std::vector<active> database::focuses(int year, int month) const {
    return actives_detail("focus_logs", std::format(" AND strftime('%Y-%m', logs.start)='{}-{}'", year, month));
}

std::vector<active> database::focuses(int year, int month, int day) const {
    return actives_detail("focus_logs", std::format(" AND strftime('%Y-%m-%d', logs.start)='{}-{}-{}'", year, month, day));
}

std::vector<ignore> database::ignores() const {
    std::vector<ignore> result;

    using select_t = std::tuple<std::string, std::string>;
    SQLite::Statement select{db_, "SELECT * FROM ignores"};
    while (select.executeStep()) {
        const auto [type_str, value] = select.getColumns<select_t, 2>();

        const ignore_type type = type_str == "file" ? ignore_file : type_str == "path" ? ignore_path : invalid;
        if (type == invalid) {
            continue;
        }
        result.push_back({type, value});
    }

    return result;
}

std::vector<active> database::actives_detail(std::string_view table, std::string_view date_filter) const {
    const std::string query_str = std::format("SELECT a.path, a.name, logs.start, logs.end FROM {} AS logs "
                                              "JOIN applications AS a ON logs.program_id = a.id AND IS_IGNORED(a.path)=0 "
                                              "WHERE 1{} "
                                              "ORDER BY a.path",
                                              table, date_filter);
    SQLite::Statement select{db_, query_str};
    return fill_actives(select);
}

std::vector<active> database::fill_actives(SQLite::Statement &select) const {
    using select_t = std::tuple<std::string, std::string, std::string, std::string>;

    std::vector<active> result;
    active              act;

    while (select.executeStep()) {
        const auto [path, name, start_str, end_str] = select.getColumns<select_t, 4>();

        if (act.path != path) {
            if (!act.path.empty() && !act.times.empty()) {
                result.push_back(act);
            }
            act.path = path;
            act.name = name;
            act.times.clear();
        }

        const auto start = parse_time(start_str);
        const auto end   = parse_time(end_str);
        act.times.push_back({start, end});
    }
    if (!act.path.empty() && !act.times.empty()) {
        result.push_back(act);
    }

    return result;
}

bool database::valid_application(const process &p) {
    if (!p.exist()) {
        return false;
    }
    const std::string path = p.full_path(), name = p.window_name();
    if (path.empty()) {
        return false;
    }
    if (is_ignored(path)) {
        return false;
    }

    // there's the INSERT OR REPLACE query but it replaces the id and it breaks the foreign key

    // update
    SQLite::Statement update{db_, "UPDATE applications SET name=? WHERE path=?"};
    update.bind(1, name);
    update.bind(2, path);
    if (update.exec()) {
        return true;
    }

    // if not exist, insert
    SQLite::Statement insert{db_, "INSERT INTO applications(path, name) VALUES (?, ?)"};
    insert.bind(1, path);
    insert.bind(2, name);
    insert.exec();
    return true;
}

bool database::is_ignored(const fs::path &path) const {
    for (const auto &[type, value]: ignores()) {
        switch (type) {
        case ignore_file:
            if (!path.compare(value)) {
                return true;
            }
            break;
        case ignore_path:
            if (in_same_directory(path, value)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

void database::is_ignored_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv) {
    if (argc != 1) {
        sqlite3_result_error(context, "Wrong number of parameters (must be 1)", -1);
        return;
    }
    const unsigned char *text = sqlite3_value_text(argv[0]);
    const int            size = sqlite3_value_bytes(argv[0]);
    if (!text || !size) {
        sqlite3_result_error(context, "The argument must be a non-empty string.", -1);
        return;
    }

    const std::string_view path(std::bit_cast<const char *>(text), size);
    const database        *self = static_cast<database *>(sqlite3_user_data(context));
    sqlite3_result_int(context, self->is_ignored(path) ? 1 : 0);
}
} // namespace apptime