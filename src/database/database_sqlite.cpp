#include "database_sqlite.hpp"

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

    void update();

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
    if (opts_.date.index() == 0) { // std::monostate
        return "logs.start";
    }
    return "CASE WHEN strftime(:date_format, logs.start) = :date_value THEN logs.start "
           "ELSE strftime('%Y-%m-%d %H:%M:%S', logs.start, :date_start, :date_next) END AS 'start'";
}

std::string select_records::logs_end() const {
    if (opts_.date.index() == 0) { // std::monostate
        return "logs.end";
    }
    return "CASE WHEN strftime(:date_format, logs.end) = :date_value THEN logs.end "
           "ELSE strftime('%Y-%m-%d %H:%M:%S', logs.end, :date_start, '-1 second') END AS 'end'";
}

std::string select_records::where() {
    struct date_info {
        std::string format, value, start, next;
    };

    struct date_info_converter {
        date_info operator()(std::chrono::year year) {
            return {
                .format = "%Y",
                .value  = std::format("{:d}", static_cast<int>(year)),
                .start  = "start of year",
                .next   = "+1 year",
            };
        }

        date_info operator()(std::chrono::year_month year_month) {
            return {
                .format = "%Y-%m",
                .value  = std::format("{:d}-{:02d}", static_cast<int>(year_month.year()), static_cast<unsigned>(year_month.month())),
                .start  = "start of month",
                .next   = "+1 month",
            };
        }

        date_info operator()(std::chrono::year_month_day year_month_day) {
            return {
                .format = "%Y-%m-%d",
                .value  = std::format("{:d}-{:02d}-{:02d}", static_cast<int>(year_month_day.year()), static_cast<unsigned>(year_month_day.month()),
                                      static_cast<unsigned>(year_month_day.day())),
                .start  = "start of day",
                .next   = "+1 day",
            };
        }

        date_info operator()(std::monostate /*unused*/) {
            return {};
        }
    };

    binds_.clear();
    std::string result = "WHERE 1";

    // day/month/year
    if (opts_.date.index() != 0) { // not std::monostate
        const date_info info = std::visit(date_info_converter{}, opts_.date);

        binds_.emplace(":date_format", info.format);
        binds_.emplace(":date_value", info.value);
        binds_.emplace(":date_start", info.start);
        binds_.emplace(":date_next", info.next);

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
database_sqlite::database_sqlite(const std::filesystem::path &path) : db_{path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE} {
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

bool database_sqlite::add_active(const record &rec) {
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

bool database_sqlite::add_focus(const record &rec) {
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

void database_sqlite::add_ignore(ignore_type type, std::string_view value) {
    const std::string type_str = ignore_to_string(type);
    if (type_str.empty()) {
        return;
    }

    SQLite::Statement insert{db_, "INSERT INTO ignores(type, value) VALUES (?, ?)"};
    insert.bind(1, type_str);
    insert.bind(2, std::string{value});
    insert.exec();
}

void database_sqlite::remove_ignore(ignore_type type, std::string_view value) {
    const std::string type_str = ignore_to_string(type);
    if (type_str.empty()) {
        return;
    }

    SQLite::Statement remove{db_, "DELETE FROM ignores WHERE type=? AND value=?"};
    remove.bind(1, type_str);
    remove.bind(2, std::string{value});
    remove.exec();
}

std::vector<record> database_sqlite::actives(const options &opt) const {
    return records_detail("active_logs", opt);
}

std::vector<record> database_sqlite::focuses(const options &opt) const {
    return records_detail("focus_logs", opt);
}

std::vector<ignore> database_sqlite::ignores() const {
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

std::vector<record> database_sqlite::records_detail(std::string_view table, const options &opt) const {
    const select_records builder{table, opt};
    SQLite::Statement    select{db_, builder.query()};
    for (const auto &[key, value]: builder.binds()) {
        select.bind(key, value);
    }
    return fill_records(select);
}

std::vector<record> database_sqlite::fill_records(SQLite::Statement &select) {
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

bool database_sqlite::valid_application(const record &rec) {
    if (rec.path.empty() || is_ignored(rec.path)) {
        return false;
    }

    SQLite::Statement update{db_, "INSERT INTO applications(path, name) VALUES (?, ?) "
                                  "ON CONFLICT(path) DO UPDATE SET name=excluded.name;"};
    update.bind(1, rec.path);
    update.bind(2, rec.name);
    return update.exec() == 1;
}

void database_sqlite::is_ignored_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv) {
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
    const auto            *self = static_cast<database_sqlite *>(sqlite3_user_data(context));
    sqlite3_result_int(context, self->is_ignored(path) ? 1 : 0);
}
} // namespace apptime