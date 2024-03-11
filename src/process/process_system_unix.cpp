#include "process/process_system.hpp"

#include <fstream>
#include <memory>

#include <signal.h> // NOLINT
#include <proc/readproc.h>

using namespace std::chrono;

constexpr std::time_t invalid_time = static_cast<std::time_t>(-1);

// TODO: X11/Wayland support for window_name, active_windows & focused_window?

std::time_t system_boot_time() {
    std::ifstream fp{"/proc/uptime"};
    if (!fp) {
        return invalid_time;
    }

    double uptime = 0;
    fp >> uptime;
    if (!fp) {
        return invalid_time;
    }

    std::time_t now = time(nullptr);
    return now - static_cast<std::time_t>(uptime);
}

namespace apptime {
bool process_system::exist() const {
    if (process_id_ == -1) {
        return false;
    }
    // If sig is 0, then no signal is sent, but existence and permission checks are still performed.
    return kill(process_id_, 0) == 0;
}

std::string process_system::window_name() const {
    return {};
}

std::string process_system::full_path() const {
    std::array<char, PATH_MAX> array = {};
    std::string                path  = std::format("/proc/{:d}/exe", process_id_);

    // read the symbolic link
    ssize_t len = readlink(path.data(), array.data(), array.size() - 1);
    if (len != -1) {
        return {array.data(), static_cast<std::size_t>(len)};
    }
    return {};
}

system_clock::time_point process_system::start() const {
    std::array               pid_list{process_id_, 0};
    proc_t                   proc_info = {};
    system_clock::time_point result;

    PROCTAB *proc = openproc(PROC_PID | PROC_FILLSTAT, pid_list.data());
    while (readproc(proc, &proc_info)) {
        if (proc_info.tid == process_id_) {
            const auto start = static_cast<std::time_t>(proc_info.start_time / sysconf(_SC_CLK_TCK)) + system_boot_time();
            result           = system_clock::from_time_t(start);
            break;
        }
    }
    closeproc(proc);

    return result;
}

std::chrono::system_clock::time_point process_system::focused_start() const {
    return start();
}

// process_system_mgr

std::vector<process_mgr::process_type> process_system_mgr::active_processes() {
    proc_t                    proc_info = {};
    std::vector<process_type> result;

    PROCTAB *proctab = openproc(PROC_FILLSTAT);
    while (readproc(proctab, &proc_info)) {
        process_system proc{proc_info.tid};
        if (proc.exist()) {
            result.emplace_back(std::make_unique<process_system>(proc));
        }
    }
    closeproc(proctab);
    return result;
}

std::vector<process_mgr::process_type> process_system_mgr::active_windows(bool /*only_visible*/) {
    return active_processes();
}

process_mgr::process_type process_system_mgr::focused_window() {
    return std::make_unique<process_system>(-1);
}
} // namespace apptime
