#ifndef APPTIME_DATABASE_HPP
#define APPTIME_DATABASE_HPP

#include <chrono>
#include <optional>
#include <filesystem>
#include <string_view>

#include <SQLiteCpp/SQLiteCpp.h>

namespace apptime {
/// @brief A structure with application-specific activities.
struct record {
    using time_point_t = std::chrono::system_clock::time_point;
    using times_t      = std::vector<std::pair<time_point_t, time_point_t>>;

    std::string path, name;
    times_t     times;
};

/// @brief Types of ignoring
enum ignore_type {
    invalid = -1,
    /// @brief Ignores primary and child paths.
    ignore_path,
    /// @brief Ignores a specific application.
    ignore_file
};
using ignore = std::pair<ignore_type, std::string>;

class database {
public:
    /// @brief Search parameters.
    struct options {
        std::string        path;
        std::optional<int> year, month, day;
    };

    /**
     * @brief Construct a new database object.
     *
     * @param path File path.
     */
    database(const std::filesystem::path &path);

    /**
     * @brief Add an active record(s) to the database.
     *
     * @param rec The active record to add.
     * @return true if the addition is successful, false otherwise.
     */
    bool add_active(const record &rec);

    /**
     * @brief Add a focus record(s) to the database.
     *
     * @param rec The focus record to add.
     * @return true if the addition is successful, false otherwise.
     */
    bool add_focus(const record &rec);

    /**
     * @brief Add an entry to the ignore list.
     *
     * @param type The type of ignoring (file or path).
     * @param value The value to be ignored.
     */
    void add_ignore(ignore_type type, std::string_view value);

    /**
     * @brief Remove an entry from the ignore list.
     *
     * @param type The type of ignoring (file or path).
     * @param value The value to be removed from the ignore list.
     */
    void remove_ignore(ignore_type type, std::string_view value);

    /**
     * @brief Retrieves a list of active records based on the provided options.
     *
     * @param opt The search options.
     * @return std::vector<record> A vector of active records.
     */
    std::vector<record> actives(const options &opt = {}) const;

    /**
     * @brief Retrieves a list of focus records based on the provided options.
     *
     * @param opt The search options.
     * @return std::vector<record> A vector of focus records.
     */
    std::vector<record> focuses(const options &opt = {}) const;

    /**
     * @brief Retrieves the current ignore list.
     *
     * @return std::vector<ignore> A vector of ignore entries.
     */
    std::vector<ignore> ignores() const;

private:
    /**
     * @brief Retrieves records based on the provided options and table name.
     *
     * @param table The table name (active_logs or focus_logs).
     * @param opt The search options.
     * @return std::vector<record> A vector of records.
     */
    std::vector<record> records_detail(std::string_view table, const options &opt) const;

    /**
     * @brief Validates whether the provided record is valid for insertion.
     *
     * This method checks if the provided record is valid for insertion into the database.
     * It verifies whether the application path is not empty, not ignored, and updates the application's name if it exists,
     * or registers a new application if it doesn't.
     *
     * @param act The record to validate.
     * @return true if the active record is valid and successfully inserted, false otherwise.
     */
    bool valid_application(const record &rec);

    /**
     * @brief Checks whether a given path is in the ignore list.
     *
     * This method examines whether the specified path is present in the list of ignored entries.
     * It iterates through each entry in the ignore list, comparing the provided path against each entry.
     * The path can be ignored based on two types:
     *   - 'file': Exact match with the specified file path.
     *   - 'path': Ignored if the specified path is within the same directory or its child paths as the entry in the ignore list.
     *
     * @param path The path to check for being ignored.
     * @return true if the path is ignored, false otherwise.
     */
    bool is_ignored(const std::filesystem::path &path) const;

    /**
     * @brief Fills the records vector based on the provided SQLite statement.
     *
     * @param stmt The SQLite statement to execute.
     * @return std::vector<record> A vector of filled records.
     */
    static std::vector<record> fill_records(SQLite::Statement &select);

    /**
     * @brief SQLite function for checking whether a path is ignored.
     *
     * @param context The SQLite context.
     * @param argc The number of arguments.
     * @param argv The SQLite values.
     */
    static void is_ignored_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv);

    /// @brief The SQLite database instance.
    SQLite::Database db_;
};
} // namespace apptime

#endif // APPTIME_DATABASE_HPP