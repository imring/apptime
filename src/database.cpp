#include "database.hpp"

#include <sqlite3.h>

namespace fs = std::filesystem;

// parse a time string in a format "%Y-%m-%d %T" into a system_clock::time_point
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

// check if current is in the primary or child directory
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

// convert options to a string for use in SQL queries.
// path:           WHERE path='...'
// year/month/day: WHERE strftime('%Y-%m-%d')='YYYY-mm-dd'
std::string options_to_string(const apptime::database::options &opt) {
    std::string result = "WHERE 1";
    if (!opt.path.empty()) {
        result += " AND path = '" + opt.path + "'";
    }
    if (opt.year || opt.month || opt.day) {
        std::string fmt, date;
        if (opt.year) {
            fmt += "%Y-";
            date = std::format("{}{:04d}-", date, opt.year.value());
        }
        if (opt.month) {
            fmt += "%m-";
            date = std::format("{}{:02d}-", date, opt.month.value());
        }
        if (opt.day) {
            fmt += "%d-";
            date = std::format("{}{:02d}-", date, opt.day.value());
        }
        if (!date.empty()) {
            fmt.pop_back();
            date.pop_back();
        }

        result = std::format("{0} AND (strftime('{1}', logs.start)='{2}' OR strftime('{1}', logs.end)='{2}')", result, fmt, date);
    }
    return result;
}

namespace apptime {
database::database(const std::filesystem::path &path) : db_{path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
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

bool database::add_active(const record &rec) {
    if (!valid_application(rec)) {
        return false;
    }

    bool              result = true;
    SQLite::Statement insert{db_, "INSERT OR REPLACE INTO active_logs (program_id, start, end) VALUES "
                                  "((SELECT id FROM applications WHERE path=?), ?, ?)"};
    for (const auto &[start, end]: rec.times) {
        const std::string start_str = std::format("{:L%F %T}", start);
        const std::string end_str   = std::format("{:L%F %T}", end);

        insert.bind(1, rec.path);
        insert.bind(2, start_str);
        insert.bind(3, end_str);
        result = result && insert.exec() == 1;

        insert.reset();
        insert.clearBindings();
    }
    return result;
}

bool database::add_focus(const record &rec) {
    if (!valid_application(rec)) {
        return false;
    }

    bool              result = true;
    SQLite::Statement insert{db_, "INSERT OR REPLACE INTO focus_logs (program_id, start, end) VALUES "
                                  "((SELECT id FROM applications WHERE path=?), ?, ?)"};
    for (const auto &[start, end]: rec.times) {
        const std::string start_str = std::format("{:L%F %T}", start);
        const std::string end_str   = std::format("{:L%F %T}", end);

        insert.bind(1, rec.path);
        insert.bind(2, start_str);
        insert.bind(3, end_str);
        result = result && insert.exec() == 1;

        insert.reset();
        insert.clearBindings();
    }
    return result;
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

std::vector<record> database::actives(const options &opt) const {
    return records_detail("active_logs", opt);
}

std::vector<record> database::focuses(const options &opt) const {
    return records_detail("focus_logs", opt);
}

std::vector<ignore> database::ignores() const {
    std::vector<ignore> result;

    using select_t = std::tuple<std::string, std::string>;
    SQLite::Statement select{db_, "SELECT type, value FROM ignores"};
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

std::vector<record> database::records_detail(std::string_view table, const options &opt) const {
    const std::string query_str = std::format("SELECT a.path, a.name, logs.start, logs.end FROM {} AS logs "
                                              "JOIN applications AS a ON logs.program_id = a.id AND IS_IGNORED(a.path)=0 "
                                              "{} "
                                              "ORDER BY a.path",
                                              table, options_to_string(opt));
    SQLite::Statement select{db_, query_str};
    return fill_records(select);
}

std::vector<record> database::fill_records(SQLite::Statement &select) const {
    using select_t = std::tuple<std::string, std::string, std::string, std::string>;

    std::vector<record> result;
    record              rec;

    while (select.executeStep()) {
        const auto [path, name, start_str, end_str] = select.getColumns<select_t, 4>();

        if (rec.path != path) {
            if (!rec.path.empty() && !rec.times.empty()) {
                result.push_back(rec);
            }
            rec.path = path;
            rec.name = name;
            rec.times.clear();
        }

        const auto start = parse_time(start_str);
        const auto end   = parse_time(end_str);
        rec.times.push_back({start, end});
    }
    if (!rec.path.empty() && !rec.times.empty()) {
        result.push_back(rec);
    }

    return result;
}

bool database::valid_application(const record &rec) {
    if (rec.path.empty() || is_ignored(rec.path)) {
        return false;
    }

    // there's the INSERT OR REPLACE query but it replaces the id and it breaks the foreign key

    // update
    SQLite::Statement update{db_, "UPDATE applications SET name=? WHERE path=?"};
    update.bind(1, rec.name);
    update.bind(2, rec.path);
    if (update.exec()) {
        return true;
    }

    // if not exist, insert
    SQLite::Statement insert{db_, "INSERT INTO applications(path, name) VALUES (?, ?)"};
    insert.bind(1, rec.path);
    insert.bind(2, rec.name);
    return insert.exec() == 1;
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