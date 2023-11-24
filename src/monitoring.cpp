#include "monitoring.hpp"
#include "process.hpp"

#include <ranges>
#include <thread>
#include <iostream>
#include <algorithm>

using namespace std::chrono_literals;

std::vector<apptime::process> filtered_processes() {
    std::vector<apptime::process> result = apptime::process::active_windows();

    const auto [first_remove, last_remove] = std::ranges::remove_if(result, [](const apptime::process &a) {
        return a.full_path().empty();
    });
    result.erase(first_remove, last_remove);

    std::ranges::sort(result, [](const apptime::process &a, const apptime::process &b) {
        const std::string apath = a.full_path(), bpath = b.full_path();
        return (apath < bpath) || (apath == bpath && a.start() < b.start());
    });

    // select oldest processes
    const auto [first_unique, last_unique] = std::ranges::unique(result, [](const apptime::process &a, const apptime::process &b) {
        return a.full_path() == b.full_path() && a.start() <= b.start();
    });
    result.erase(first_unique, last_unique);

    return result;
}

apptime::record build_record(const apptime::process &proc, bool focused = false) {
    apptime::record result;
    result.name = proc.window_name();
    result.path = proc.full_path();
    result.times.emplace_back(focused ? proc.focused_start() : proc.start(), std::chrono::system_clock::now());
    return result;
}

namespace apptime {
monitoring::monitoring(database &db) : db_{db}, active_delay{5s}, focus_delay{1s}, running_{false} {}

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
        for (const auto &v: filtered_processes()) {
            db_.add_active(build_record(v));
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
        db_.add_focus(build_record(process::focused_window(), true));
    }
}
} // namespace apptime