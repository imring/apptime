#ifndef APPTIME_MONITORING_HPP
#define APPTIME_MONITORING_HPP

#include <mutex>
#include <atomic>
#include <chrono>

#include "database.hpp"

namespace apptime {
class monitoring {
public:
    monitoring(database &db);

    void start();
    void stop();

    std::chrono::milliseconds active_delay, focus_delay;

private:
    void active_thread();
    void focus_thread();

    database        &db_;
    std::mutex       mutex_;
    std::atomic_bool running_;
};
} // namespace apptime

#endif // APPTIME_MONITORING_HPP