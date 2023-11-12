#ifndef APPTIME_DATABASE_HPP
#define APPTIME_DATABASE_HPP

#include <chrono>
#include <filesystem>

#include <SQLiteCpp/SQLiteCpp.h>

#include "process.hpp"

namespace apptime {
struct active {
    using time_point_t = std::chrono::system_clock::time_point;
    using times_t      = std::vector<std::pair<time_point_t, time_point_t>>;

    std::string path, name;
    times_t     times;
};

enum ignore_type { invalid = -1, ignore_path, ignore_file };
using ignore = std::pair<ignore_type, std::string>;

class database {
public:
    database(std::string_view path);

    void add_active(const process &p);
    void add_focus(const process &p);

    void add_ignore(ignore_type type, std::string_view value);
    void remove_ignore(ignore_type type, std::string_view value);

    std::vector<active> actives() const;
    std::vector<active> actives(int year) const;
    std::vector<active> actives(int year, int month) const;
    std::vector<active> actives(int year, int month, int day) const;

    std::vector<active> focuses() const;
    std::vector<active> focuses(int year) const;
    std::vector<active> focuses(int year, int month) const;
    std::vector<active> focuses(int year, int month, int day) const;

    std::vector<ignore> ignores() const;

private:
    std::vector<active> actives_detail(std::string_view table, std::string_view date_filter = {}) const;
    std::vector<active> fill_actives(SQLite::Statement &stmt) const;

    bool valid_application(const process &p);
    bool is_ignored(const std::filesystem::path &path) const;

    static void is_ignored_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv);

    SQLite::Database db_;
};
} // namespace apptime

#endif // APPTIME_DATABASE_HPP