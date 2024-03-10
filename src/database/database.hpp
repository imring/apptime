#ifndef APPTIME_DATABASE_HPP
#define APPTIME_DATABASE_HPP

#include <chrono>
#include <variant>

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
        std::string                                                                                           path;
        std::variant<std::monostate, std::chrono::year, std::chrono::year_month, std::chrono::year_month_day> date;
    };

    virtual ~database() = default;

    /**
     * @brief Add an active record(s) to the database.
     *
     * @param rec The active record to add.
     * @return true if the addition is successful, false otherwise.
     */
    virtual bool add_active(const record &rec) = 0;

    /**
     * @brief Add a focus record(s) to the database.
     *
     * @param rec The focus record to add.
     * @return true if the addition is successful, false otherwise.
     */
    virtual bool add_focus(const record &rec) = 0;

    /**
     * @brief Add an entry to the ignore list.
     *
     * @param type The type of ignoring (file or path).
     * @param value The value to be ignored.
     */
    virtual void add_ignore(ignore_type type, std::string_view value) = 0;

    /**
     * @brief Remove an entry from the ignore list.
     *
     * @param type The type of ignoring (file or path).
     * @param value The value to be removed from the ignore list.
     */
    virtual void remove_ignore(ignore_type type, std::string_view value) = 0;

    /**
     * @brief Retrieves a list of active records based on the provided options.
     *
     * @param opt The search options.
     * @return std::vector<record> A vector of active records.
     */
    virtual std::vector<record> actives(const options &opt) const = 0;

    /**
     * @brief Retrieves a list of focus records based on the provided options.
     *
     * @param opt The search options.
     * @return std::vector<record> A vector of focus records.
     */
    virtual std::vector<record> focuses(const options &opt) const = 0;

    /**
     * @brief Retrieves the current ignore list.
     *
     * @return std::vector<ignore> A vector of ignore entries.
     */
    virtual std::vector<ignore> ignores() const = 0;
};
} // namespace apptime

#endif // APPTIME_DATABASE_HPP