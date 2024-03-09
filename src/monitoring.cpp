#include "monitoring.hpp"
#include "platforms/process.hpp"

#include <ranges>
#include <thread>
#include <unordered_map>
#include <utility>

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds default_active_delay = 5s;
constexpr std::chrono::milliseconds default_focus_delay  = 1s;

// monitoring writes processes that have a window. among identical processes writes the oldest.
std::vector<apptime::process> filtered_windows() {
    std::unordered_map<std::string, apptime::process> result;
    for (const auto &win: apptime::process::active_windows(true)) {
        const std::string path = win.full_path();
        if (path.empty()) {
            continue;
        }

        const auto it = result.find(path);
        if (it == result.end()) {
            // write a new process
            result.emplace(path, win);
            continue;
        }

        // select the oldest process
        if (win.start() > it->second.start()) {
            it->second = win;
        }
    }

    const auto values = result | std::views::values;
    return std::vector<apptime::process>{values.begin(), values.end()};
}

apptime::record build_record(const apptime::process &proc, bool focused = false) {
    apptime::record result;
    result.name = proc.window_name();
    result.path = proc.full_path();
    result.times.emplace_back(focused ? proc.focused_start() : proc.start(), std::chrono::system_clock::now());
    return result;
}

namespace apptime {
monitoring::monitoring(std::shared_ptr<database> db)
    : db_{std::move(db)},
      active_delay{default_active_delay},
      focus_delay{default_focus_delay},
      running_{false} {}

monitoring::~monitoring() {
    stop();
}

void monitoring::start() {
    running_       = true;
    active_thread_ = std::thread{&monitoring::active_thread, this};
    focus_thread_  = std::thread{&monitoring::focus_thread, this};
}

void monitoring::stop() {
    running_ = false;

    // this is needed to stop threads immediately
    cv.notify_all();
    if (active_thread_.joinable()) {
        active_thread_.join();
    }
    if (focus_thread_.joinable()) {
        focus_thread_.join();
    }
}

bool monitoring::running() const {
    return running_;
}

void monitoring::active_thread() {
    while (running()) {
        std::unique_lock<std::mutex> lock{mutex_};
        if (cv.wait_for(lock, active_delay, [this] {
                return !running();
            })) {
            return;
        }
        for (const auto &proc: filtered_windows()) {
            db_->add_active(build_record(proc));
        }
    }
}

void monitoring::focus_thread() {
    while (running()) {
        std::unique_lock<std::mutex> lock{mutex_};
        if (cv.wait_for(lock, focus_delay, [this] {
                return !running();
            })) {
            return;
        }
        db_->add_focus(build_record(process::focused_window(), true));
    }
}
} // namespace apptime