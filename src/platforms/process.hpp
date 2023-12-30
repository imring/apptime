#ifndef APPTIME_PROCESS_HPP
#define APPTIME_PROCESS_HPP

#include <vector>
#include <string>
#include <chrono>

namespace apptime {
/// @brief The class represent a system process and provides functionality to get information about it.
class process {
public:
    /**
     * @brief Construct a new process object with the given process ID.
     *
     * @param pid The process ID.
     */
    process(int pid) : process_id_{pid} {}

    /**
     * @brief Check if the process exists.
     *
     * @return true if the process exists, false otherwise.
     */
    bool exist() const;

    /**
     * @brief Get the window name associated with the process.
     *
     * @return std::string The window name.
     */
    std::string window_name() const;

    /**
     * @brief Get the full path of the executable associated with the process.
     *
     * @return std::string The full path of the executable.
     */
    std::string full_path() const;

    /**
     * @brief Get the start time of the process.
     *
     * @return std::chrono::system_clock::time_point The start time of the process.
     */
    std::chrono::system_clock::time_point start() const;

    /**
     * @brief Get the start time of the focused window.
     *
     * @warning This function must be used in conjunction with `process::focused_window`.
     *
     * @return std::chrono::system_clock::time_point The start time of the focused window.
     */
    std::chrono::system_clock::time_point focused_start() const {
        return focused_start_;
    }

    /**
     * @brief Get a list of all active processes.
     *
     * @return std::vector<process> A vector of active processes.
     */
    static std::vector<process> active_processes();

    /**
     * @brief Get a list of all active processes that have a window.
     *
     * @return std::vector<process> A vector of active windows.
     */
    static std::vector<process> active_windows();

    /**
     * @brief Get a process that has a focused window.
     *
     * @return process A process that has a focused window.
     */
    static process focused_window();

private:
    /// @brief The process ID.
    int process_id_;
    /// @brief The start time of the focused window (see process::focused_start).
    std::chrono::system_clock::time_point focused_start_;
};
} // namespace apptime

#endif // APPTIME_PROCESS_HPP