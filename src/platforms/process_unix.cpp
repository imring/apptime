#include "process.hpp"

#include <signal.h>
#include <proc/readproc.h>

using namespace std::chrono;

constexpr std::time_t invalid_time = static_cast<std::time_t>(-1);

// TODO: X11 support for window_name, active_windows & focused_window

std::time_t system_boot_time() {
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) {
        return invalid_time;
    }

    double uptime;
    if (fscanf(fp, "%lf", &uptime) != 1) {
        fclose(fp);
        return invalid_time;
    }
    fclose(fp);

    std::time_t now = time(nullptr);
    return now - static_cast<std::time_t>(uptime);
}

namespace apptime {
bool process::exist() const {
    if (process_id_ == -1) {
        return false;
    }
    // If sig is 0, then no signal is sent, but existence and permission checks are still performed.
    return kill(process_id_, 0) == 0;
}

std::string process::window_name() const {
    return {};
}

std::string process::full_path() const {
    char        result[PATH_MAX];
    std::string path = std::format("/proc/{:d}/exe", process_id_);

    // read the symbolic link
    ssize_t len = readlink(path.data(), result, sizeof(result) - 1);
    if (len != -1) {
        result[len] = '\0';
        return {result};
    }
    return {};
}

system_clock::time_point process::start() const {
    pid_t                    pid_list[] = {process_id_, 0};
    proc_t                   proc_info  = {};
    system_clock::time_point result;

    PROCTAB *proc = openproc(PROC_PID | PROC_FILLSTAT, &pid_list);
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

// static

std::vector<process> process::active_processes() {
    proc_t               proc_info = {};
    std::vector<process> result;

    PROCTAB *proc = openproc(PROC_FILLSTAT);
    while (readproc(proc, &proc_info)) {
        process p{proc_info.tid};
        if (p.exist()) {
            result.push_back(p);
        }
    }
    closeproc(proc);
    return result;
}

std::vector<process> process::active_windows(bool only_visible) {
    return active_processes();
}

process process::focused_window() {
    return process{-1};
}
} // namespace apptime
