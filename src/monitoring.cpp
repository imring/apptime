#include "monitoring.hpp"

#include <ranges>
#include <thread>
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

    const auto [first_unique, last_unique] = std::ranges::unique(result, [](const apptime::process &a, const apptime::process &b) {
        return a.full_path() == b.full_path() && a.start() <= b.start();
    });
    result.erase(first_unique, last_unique);

    return result;
}

namespace apptime {
monitoring::monitoring(database &db) : db_{db}, active_delay{5s}, focus_delay{1s}, running_{false} {}

void monitoring::start() {
    running_ = true;
    std::thread{&monitoring::active_thread, this}.detach();
    std::thread{&monitoring::focus_thread, this}.detach();
}

void monitoring::stop() {
    running_ = false;
}

void monitoring::active_thread() {
    while (running_) {
        std::chrono::milliseconds delay;
        {
            std::scoped_lock<std::mutex> lock{mutex_};
            for (const auto &v: filtered_processes()) {
                db_.add_active(v);
            }
            delay = active_delay;
        }
        std::this_thread::sleep_for(delay);
    }
}

void monitoring::focus_thread() {
    while (running_) {
        std::chrono::milliseconds delay;
        {
            std::scoped_lock<std::mutex> lock{mutex_};
            db_.add_focus(process::focused_window());
            delay = focus_delay;
        }
        std::this_thread::sleep_for(delay);
    }
}
} // namespace apptime