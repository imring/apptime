#ifndef APPTIME_MONITORING_HPP
#define APPTIME_MONITORING_HPP

#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>

#include "database.hpp"

namespace apptime {
class monitoring {
public:
    monitoring(database &db);
    ~monitoring();

    void start();
    void stop();

    bool running() const;

    std::chrono::milliseconds active_delay, focus_delay;

private:
    void active_thread();
    void focus_thread();

    std::thread active_thread_;
    std::thread focus_thread_;

    database        &db_;
    std::mutex       mutex_;
    std::atomic_bool running_;

    std::condition_variable cv;
};
} // namespace apptime

#endif // APPTIME_MONITORING_HPP