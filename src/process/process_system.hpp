#ifndef APPTIME_PROCESS_DEFAULT_HPP
#define APPTIME_PROCESS_DEFAULT_HPP

#include "process.hpp"

namespace apptime {
/// @brief The class represent a system process and provides functionality to get information about it
/// using WinAPI for Windows and POSIX for Linux.
class process_system : public process {
public:
    /**
     * @brief Construct a new process object with the given process ID.
     *
     * @param pid The process ID.
     */
    explicit process_system(int pid) : process_id_{pid} {}

    /**
     * @brief Check if the process exists.
     *
     * @return true if the process exists, false otherwise.
     */
    bool exist() const override;

    /**
     * @brief Get the window name associated with the process.
     *
     * @return std::string The window name.
     */
    std::string window_name() const override;

    /**
     * @brief Get the full path of the executable associated with the process.
     *
     * @return std::string The full path of the executable.
     */
    std::string full_path() const override;

    /**
     * @brief Get the start time of the process.
     *
     * @return std::chrono::system_clock::time_point The start time of the process.
     */
    std::chrono::system_clock::time_point start() const override;

    /**
     * @brief Get the start time of the focused window.
     *
     * @warning For Windows, this function must be used in conjunction with `process::focused_window`.
     *
     * @return std::chrono::system_clock::time_point The start time of the focused window.
     */
    std::chrono::system_clock::time_point focused_start() const override;

private:
    /// @brief The process ID.
    int process_id_;
#ifdef _WIN32
    /// @brief The start time of the focused window (see process::focused_start).
    std::chrono::system_clock::time_point focused_start_;
#endif

    friend class process_system_mgr;
};

class process_system_mgr : public process_mgr {
public:
    /**
     * @brief Get a list of all active processes.
     *
     * @return std::vector<process_type> A vector of active processes.
     */
    std::vector<process_type> active_processes() override;

    /**
     * @brief Get a list of all active processes that have a window.
     *
     * @param only_visible Get only visible windows.
     *
     * @return std::vector<process_type> A vector of active windows.
     */
    std::vector<process_type> active_windows(bool only_visible) override;

    /**
     * @brief Get a process that has a focused window.
     *
     * @return process_type A process that has a focused window.
     */
    process_type focused_window() override;
};
} // namespace apptime

#endif // APPTIME_PROCESS_DEFAULT_HPP