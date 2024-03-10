#ifndef APPTIME_MONITORING_HPP
#define APPTIME_MONITORING_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "database/database.hpp"
#include "process/process.hpp"

namespace apptime {
class monitoring {
public:
    monitoring(std::shared_ptr<database> db, std::unique_ptr<process_mgr> manager);
    ~monitoring();

    void start();
    void stop();

    bool running() const;

    std::chrono::milliseconds active_delay, focus_delay; // NOLINT

private:
    void active_thread();
    void focus_thread();

    std::thread active_thread_;
    std::thread focus_thread_;

    std::shared_ptr<database>    db_;
    std::unique_ptr<process_mgr> manager_;

    std::mutex              mutex_;
    std::atomic_bool        running_;
    std::condition_variable cv;
};
} // namespace apptime

#endif // APPTIME_MONITORING_HPP