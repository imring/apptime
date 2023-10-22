#ifndef APPTIME_PROCESS_HPP
#define APPTIME_PROCESS_HPP

#include <vector>
#include <string>
#include <chrono>

namespace apptime {
class process {
public:
    process(int pid) : process_id_{pid} {}

    bool                                  exist() const;
    std::string                           window_name() const;
    std::string                           full_path() const;
    std::chrono::system_clock::time_point start() const;

    static std::vector<process> active_processes();
    static std::vector<process> active_windows();
    static process              focused_window();

private:
    int process_id_;
};
} // namespace apptime

#endif // APPTIME_PROCESS_HPP