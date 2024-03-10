#ifndef APPTIME_PROCESS_HPP
#define APPTIME_PROCESS_HPP

#include <chrono>
#include <string>
#include <vector>

namespace apptime {
/// @brief The interface represent a system process and provides functionality to get information about it.
class process {
public:
    virtual ~process() = default;

    /**
     * @brief Check if the process exists.
     *
     * @return true if the process exists, false otherwise.
     */
    virtual bool exist() const = 0;

    /**
     * @brief Get the window name associated with the process.
     *
     * @return std::string The window name.
     */
    virtual std::string window_name() const = 0;

    /**
     * @brief Get the full path of the executable associated with the process.
     *
     * @return std::string The full path of the executable.
     */
    virtual std::string full_path() const = 0;

    /**
     * @brief Get the start time of the process.
     *
     * @return std::chrono::system_clock::time_point The start time of the process.
     */
    virtual std::chrono::system_clock::time_point start() const = 0;

    /**
     * @brief Get the start time of the focused window.
     *
     * @return std::chrono::system_clock::time_point The start time of the focused window.
     */
    virtual std::chrono::system_clock::time_point focused_start() const = 0;
};

class process_mgr {
public:
    virtual ~process_mgr() = default;

    using process_type = std::unique_ptr<process>;

    /**
     * @brief Get a list of all active processes.
     *
     * @return std::vector<process_type> A vector of active processes.
     */
    virtual std::vector<process_type> active_processes() = 0;

    /**
     * @brief Get a list of all active processes that have a window.
     *
     * @param only_visible Get only visible windows.
     *
     * @return std::vector<process_type> A vector of active windows.
     */
    virtual std::vector<process_type> active_windows(bool only_visible) = 0;

    /**
     * @brief Get a process that has a focused window.
     *
     * @return process_type A process that has a focused window.
     */
    virtual process_type focused_window() = 0;
};
} // namespace apptime

#endif // APPTIME_PROCESS_HPP