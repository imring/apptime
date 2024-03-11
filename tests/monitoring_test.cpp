// NOLINTBEGIN(readability-function-cognitive-complexity, misc-use-anonymous-namespace, cppcoreguidelines-avoid-do-while)

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "monitoring.hpp"
#include "utils.hpp"

using namespace std::chrono_literals;

class database_mock : public apptime::database {
public:
    bool add_active(const apptime::record &rec) override {
        if (is_ignored(rec.path)) {
            return false;
        }
        actives_.emplace_back(rec);
        return true;
    }

    bool add_focus(const apptime::record &rec) override {
        if (is_ignored(rec.path)) {
            return false;
        }
        focuses_.emplace_back(rec);
        return true;
    }

    void add_ignore(apptime::ignore_type type, std::string_view value) override { ignores_.emplace_back(type, value); }

    void remove_ignore(apptime::ignore_type type, std::string_view value) override {
        std::erase_if(ignores_, [type, value](const auto &ignore) {
            return ignore.first == type && ignore.second == value;
        });
    }

    std::vector<apptime::record> actives(const apptime::database::options & /*opt*/) const override { return actives_; }
    std::vector<apptime::record> focuses(const apptime::database::options & /*opt*/) const override { return focuses_; }
    std::vector<apptime::ignore> ignores() const override { return ignores_; }

private:
    std::vector<apptime::record> actives_;
    std::vector<apptime::record> focuses_;
    std::vector<apptime::ignore> ignores_;
};

class process_mock : public apptime::process {
public:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    process_mock(std::string_view window_name, std::string_view full_path, std::chrono::system_clock::time_point start)
        : window_name_{window_name},
          full_path_{full_path},
          start_{start} {}

    bool exist() const override { return true; }

    std::string window_name() const override { return window_name_; }
    std::string full_path() const override { return full_path_; }

    std::chrono::system_clock::time_point start() const override { return start_; }
    std::chrono::system_clock::time_point focused_start() const override { return start_; }

private:
    std::string                           window_name_;
    std::string                           full_path_;
    std::chrono::system_clock::time_point start_;
};

class process_mgr_mock : public apptime::process_mgr {
public:
    static process_mock make_process() { return {"test", "/dir/test", std::chrono::system_clock::now()}; }

    std::vector<process_type> active_processes() override {
        std::vector<process_type> result;
        result.emplace_back(std::make_unique<process_mock>(make_process()));
        return result;
    }
    std::vector<process_type> active_windows(bool /*only_visible*/) override { return active_processes(); }
    process_type              focused_window() override { return std::make_unique<process_mock>(make_process()); }
};

TEST_CASE("monitoring") {
    auto                database = std::make_shared<database_mock>();
    apptime::monitoring monitoring{database, std::make_unique<process_mgr_mock>()};

    SECTION("start and stop") {
        monitoring.start();
        REQUIRE(monitoring.running());

        monitoring.stop();
        REQUIRE(!monitoring.running());
    }

    SECTION("write to database") {
        constexpr auto monitoring_delay = 100ms;
        monitoring.active_delay         = monitoring_delay;
        monitoring.focus_delay          = monitoring_delay;

        monitoring.start();
        REQUIRE(monitoring.running());

        // wait for active & focus
        const int   max_records = 2;
        const timer monitoring_timer{monitoring_delay * max_records};
        while ((database->actives({}).size() != max_records || database->focuses({}).size() != max_records) && !monitoring_timer.expired()) {
            std::this_thread::sleep_for(monitoring_delay);
        }

        const std::vector<apptime::record> actives = database->actives({});
        const std::vector<apptime::record> focuses = database->focuses({});

        REQUIRE(actives.size() == max_records);
        REQUIRE(focuses.size() == max_records);

        monitoring.stop();
        REQUIRE(!monitoring.running());
    }

    SECTION("write to database with ignore") {
        constexpr auto monitoring_delay = 100ms;
        monitoring.active_delay         = monitoring_delay;
        monitoring.focus_delay          = monitoring_delay;

        database->add_ignore(apptime::ignore_type::ignore_file, process_mgr_mock::make_process().full_path());

        monitoring.start();
        REQUIRE(monitoring.running());

        // wait for active & focus
        const timer monitoring_timer{monitoring_delay};
        while ((!database->actives({}).empty() || !database->focuses({}).empty()) && !monitoring_timer.expired()) {
            std::this_thread::sleep_for(monitoring_delay);
        }

        const std::vector<apptime::record> actives = database->actives({});
        const std::vector<apptime::record> focuses = database->focuses({});

        REQUIRE(actives.empty());
        REQUIRE(focuses.empty());
    }
}

int main(int argc, char *argv[]) {
    return Catch::Session().run(argc, argv);
}

// NOLINTEND(readability-function-cognitive-complexity, misc-use-anonymous-namespace, cppcoreguidelines-avoid-do-while)