#ifndef APPTIME_DATABASE_SQLITE_HPP
#define APPTIME_DATABASE_SQLITE_HPP

#include "database.hpp"

namespace apptime {
class database_sqlite : public database {
public:
    /**
     * @brief Construct a new database object.
     *
     * @param path File path.
     */
    explicit database_sqlite(const std::filesystem::path &path);

    ~database_sqlite() override = default;

    /**
     * @brief Add an active record(s) to the database.
     *
     * @param rec The active record to add.
     * @return true if the addition is successful, false otherwise.
     */
    bool add_active(const record &rec) override;

    /**
     * @brief Add a focus record(s) to the database.
     *
     * @param rec The focus record to add.
     * @return true if the addition is successful, false otherwise.
     */
    bool add_focus(const record &rec) override;

    /**
     * @brief Add an entry to the ignore list.
     *
     * @param type The type of ignoring (file or path).
     * @param value The value to be ignored.
     */
    void add_ignore(ignore_type type, std::string_view value) override;

    /**
     * @brief Remove an entry from the ignore list.
     *
     * @param type The type of ignoring (file or path).
     * @param value The value to be removed from the ignore list.
     */
    void remove_ignore(ignore_type type, std::string_view value) override;

    /**
     * @brief Retrieves a list of active records based on the provided options.
     *
     * @param opt The search options.
     * @return std::vector<record> A vector of active records.
     */
    std::vector<record> actives(const options &opt) const override;

    /**
     * @brief Retrieves a list of focus records based on the provided options.
     *
     * @param opt The search options.
     * @return std::vector<record> A vector of focus records.
     */
    std::vector<record> focuses(const options &opt) const override;

    /**
     * @brief Retrieves the current ignore list.
     *
     * @return std::vector<ignore> A vector of ignore entries.
     */
    std::vector<ignore> ignores() const override;

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

#endif // APPTIME_DATABASE_SQLITE_HPP