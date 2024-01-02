#include "database.hpp"

#include <unordered_map>

#include <sqlite3.h>

namespace fs = std::filesystem;

// a simple builder for database::active and database::focuses
// the main purpose of the class is to provide customization of settings in sql query
class select_records {
public:
    select_records(std::string_view table_name, const apptime::database::options &opts) : table_name_{table_name}, opts_{opts} {
        update();
    }

    void update();

    std::string query() const {
        return query_;
    }
    std::unordered_map<std::string, std::string> binds() const {
        return binds_;
    }

private:
    // logs_start and logs_end are necessary for time alignment.
    // example: date in options = "2023-10-24", one of processes has start at 2023-10-24 23:58:59 and end at 2023-10-25 00:05:01.
    // these functions add conditions to return only the data for 2023-10-24 (start = 2023-10-24 23:58:59, end = 2023-10-24 23:59:59).
    std::string logs_start() const;
    std::string logs_end() const;
    std::string where();

    // input
    std::string                table_name_;
    apptime::database::options opts_;

    // output
    std::string                                  query_;
    std::unordered_map<std::string, std::string> binds_;
};

void select_records::update() {
    query_ = std::format("SELECT a.path, a.name, {}, {} FROM {} AS logs "
                         "JOIN applications AS a ON logs.program_id = a.id AND IS_IGNORED(a.path)=0 "
                         "{} "
                         "ORDER BY a.path",
                         logs_start(), logs_end(), table_name_, where());
}

std::string select_records::logs_start() const {
    if (!opts_.year) {
        return "logs.start";
    }
    return "CASE WHEN strftime(:date_format, logs.start) = :date_value THEN logs.start "
           "ELSE strftime('%Y-%m-%d %H:%M:%S', logs.start, :date_start, :date_next) END AS 'start'";
}

std::string select_records::logs_end() const {
    if (!opts_.year) {
        return "logs.start";
    }
    return "CASE WHEN strftime(:date_format, logs.end) = :date_value THEN logs.end "
           "ELSE strftime('%Y-%m-%d %H:%M:%S', logs.end, :date_start, '-1 second') END AS 'end'";
}

std::string select_records::where() {
    binds_.clear();
    std::string result = "WHERE 1";

    // day/month/year
    std::string date_format, date_value, date_start, date_next;
    if (opts_.day && opts_.month && opts_.year) {
        date_format = "%Y-%m-%d";
        date_value  = std::format("{:d}-{:02d}-{:02d}", opts_.year.value(), opts_.month.value(), opts_.day.value());
        date_start  = "start of day";
        date_next   = "+1 day";
    } else if (opts_.month && opts_.year) {
        date_format = "%Y-%m";
        date_value  = std::format("{:d}-{:02d}", opts_.year.value(), opts_.month.value());
        date_start  = "start of month";
        date_next   = "+1 month";
    } else if (opts_.year) {
        date_format = "%Y";
        date_value  = std::format("{:d}", opts_.year.value());
        date_start  = "start of year";
        date_next   = "+1 year";
    }
    if (!date_format.empty()) {
        binds_.emplace(":date_format", date_format);
        binds_.emplace(":date_value", date_value);
        binds_.emplace(":date_start", date_start);
        binds_.emplace(":date_next", date_next);

        std::format_to(std::back_inserter(result), " AND (strftime(:date_format, logs.start)=:date_value OR strftime(:date_format, logs.end)=:date_value)");
    }

    // path
    if (!opts_.path.empty()) {
        binds_.emplace(":path", opts_.path);
        std::format_to(std::back_inserter(result), " AND path=:path");
    }

    return result;
}

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

std::string ignore_to_string(apptime::ignore_type value) {
    // clang-format off
    static const std::unordered_map<apptime::ignore_type, std::string> ignores = {
        { apptime::ignore_path, "path" },
        { apptime::ignore_file, "file" }
    };
    // clang-format on
    if (auto it = ignores.find(value); it != ignores.end()) {
        return it->second;
    }
    return "";
}

apptime::ignore_type string_to_enum(std::string_view value) {
    // clang-format off
    static const std::unordered_map<std::string_view, apptime::ignore_type> ignores = {
        { "path", apptime::ignore_path },
        { "file", apptime::ignore_file }
    };
    // clang-format on
    if (auto it = ignores.find(value); it != ignores.end()) {
        return it->second;
    }
    return apptime::invalid;
}

namespace apptime {
database::database(const std::filesystem::path &path) : db_{path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
    // create the applications table
    db_.exec("CREATE TABLE IF NOT EXISTS applications ("
             "id INTEGER NOT NULL,"
             "path TEXT NOT NULL UNIQUE,"
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

    SQLite::Transaction transaction{db_};
    bool                result = true;
    SQLite::Statement   insert{db_, "INSERT OR REPLACE INTO active_logs (program_id, start, end) VALUES "
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

    transaction.commit();
    return result;
}

bool database::add_focus(const record &rec) {
    if (!valid_application(rec)) {
        return false;
    }

    SQLite::Transaction transaction{db_};
    bool                result = true;
    SQLite::Statement   insert{db_, "INSERT OR REPLACE INTO focus_logs (program_id, start, end) VALUES "
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

    transaction.commit();
    return result;
}

void database::add_ignore(ignore_type type, std::string_view value) {
    const std::string type_str = ignore_to_string(type);
    if (type_str.empty()) {
        return;
    }

    SQLite::Statement insert{db_, "INSERT INTO ignores(type, value) VALUES (?, ?)"};
    insert.bind(1, type_str);
    insert.bind(2, std::string{value});
    insert.exec();
}

void database::remove_ignore(ignore_type type, std::string_view value) {
    const std::string type_str = ignore_to_string(type);
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

        const ignore_type type = string_to_enum(type_str);
        if (type == invalid) {
            continue;
        }
        result.emplace_back(type, value);
    }

    return result;
}

std::vector<record> database::records_detail(std::string_view table, const options &opt) const {
    const select_records builder{table, opt};
    SQLite::Statement    select{db_, builder.query()};
    for (const auto &[key, value]: builder.binds()) {
        select.bind(key, value);
    }
    return fill_records(select);
}

std::vector<record> database::fill_records(SQLite::Statement &select) {
    using select_t = std::tuple<std::string, std::string, std::string, std::string>;

    std::vector<record> result;
    record              rec;

    while (select.executeStep()) {
        const auto [path, name, start_str, end_str] = select.getColumns<select_t, 4>();

        if (rec.path != path) {
            if (!rec.path.empty() && !rec.times.empty()) {
                result.emplace_back(rec);
            }
            rec.path = path;
            rec.name = name;
            rec.times.clear();
        }

        const auto start = parse_time(start_str);
        const auto end   = parse_time(end_str);
        rec.times.emplace_back(start, end);
    }
    if (!rec.path.empty() && !rec.times.empty()) {
        result.emplace_back(rec);
    }

    return result;
}

bool database::valid_application(const record &rec) {
    if (rec.path.empty() || is_ignored(rec.path)) {
        return false;
    }

    SQLite::Statement update{db_, "INSERT INTO applications(path, name) VALUES (?, ?) "
                                  "ON CONFLICT(path) DO UPDATE SET name=excluded.name;"};
    update.bind(1, rec.path);
    update.bind(2, rec.name);
    return update.exec() == 1;
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
    const auto            *self = static_cast<database *>(sqlite3_user_data(context));
    sqlite3_result_int(context, self->is_ignored(path) ? 1 : 0);
}
} // namespace apptime