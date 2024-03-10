#include "monitoring.hpp"

#include <ranges>
#include <thread>
#include <unordered_map>
#include <utility>

using namespace std::chrono_literals;

constexpr std::chrono::milliseconds default_active_delay = 5s;
constexpr std::chrono::milliseconds default_focus_delay  = 1s;

using apptime::process_mgr;

// monitoring writes processes that have a window. among identical processes writes the oldest.
std::vector<process_mgr::process_type> filtered_windows(process_mgr *manager) {
    std::unordered_map<std::string, process_mgr::process_type> result;
    for (auto &win: manager->active_windows(true)) {
        const std::string path = win->full_path();
        if (path.empty()) {
            continue;
        }

        const auto it = result.find(path);
        if (it == result.end()) {
            // write a new process
            result.emplace(path, std::move(win));
            continue;
        }

        // select the oldest process
        if (win->start() > it->second->start()) {
            it->second = std::move(win);
        }
    }

    const auto values = result | std::views::values;
    return std::vector<process_mgr::process_type>{std::make_move_iterator(values.begin()), std::make_move_iterator(values.end())};
}

apptime::record build_record(std::unique_ptr<apptime::process> proc, bool focused = false) {
    apptime::record result;
    result.name = proc->window_name();
    result.path = proc->full_path();
    result.times.emplace_back(focused ? proc->focused_start() : proc->start(), std::chrono::system_clock::now());
    return result;
}

namespace apptime {
monitoring::monitoring(std::shared_ptr<database> db, std::unique_ptr<process_mgr> manager)
    : active_delay{default_active_delay},
      focus_delay{default_focus_delay},
      db_{std::move(db)},
      manager_{std::move(manager)},
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
    return running_ && active_thread_.joinable() && focus_thread_.joinable();
}

void monitoring::active_thread() {
    for (;;) {
        std::unique_lock<std::mutex> lock{mutex_};

        // write active processes
        for (auto &proc: filtered_windows(manager_.get())) {
            db_->add_active(build_record(std::move(proc)));
        }

        // wait for next cycle
        // if monitoring is stopped, return
        if (cv.wait_for(lock, active_delay, [this] {
                return !running();
            })) {
            return;
        }
    }
}

void monitoring::focus_thread() {
    for (;;) {
        std::unique_lock<std::mutex> lock{mutex_};

        // write a focused process
        db_->add_focus(build_record(manager_->focused_window(), true));

        // wait for next cycle
        // if monitoring is stopped, return
        if (cv.wait_for(lock, focus_delay, [this] {
                return !running();
            })) {
            return;
        }
    }
}
} // namespace apptime