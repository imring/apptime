#include <algorithm>
#include <chrono>
#include <filesystem>

#include <SQLiteCpp/SQLiteCpp.h>
#include <sqlite3.h>

#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "utils.hpp"

#include "database.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

namespace fs = std::filesystem;

std::vector<apptime::record> processes;

const struct {
    fs::path ele_add    = fs::temp_directory_path() / "apptime_ele_add.db";
    fs::path ele_search = fs::temp_directory_path() / "apptime_ele_search.db";
} test_paths;

template <typename It = std::vector<apptime::record>::iterator>
bool path_is_unique(std::string_view path, It begin, It end) {
    const fs::path fs_path = fs::path{path}.parent_path();
    return std::find_if(begin, end, [&fs_path](const apptime::record &act) {
               return fs::path{act.path}.parent_path() == fs_path;
           }) == end;
}

std::vector<apptime::record> random_processes(int size) {
    std::vector<apptime::record> result;
    result.resize(size);
    for (auto it = result.begin(); it != result.end(); it++) {
        auto &val = *it;

        // random path generating (e.g. /foo/bar)
        constexpr int name_len = 10;
        for (int subfolders = 2; subfolders > 0 || !path_is_unique(val.path, result.begin(), it); subfolders--) {
            // subfolders = 2 means 1 subfolder + 1 file
            val.path += "/" + random_string(random(1, name_len));
        }

        // random name generating
        val.name = random_string(random(1, name_len));

        // random time generating
        constexpr int min_year    = 2021;
        constexpr int max_year    = 2023;
        constexpr int year_offset = 1900; // years since 1900 (see https://en.cppreference.com/w/cpp/chrono/c/tm)

        constexpr int max_month = 11;
        constexpr int max_day   = 28; // 28 because of February
        constexpr int hour      = 12;

        std::tm date = {};
        date.tm_year = random(min_year, max_year) - year_offset;
        date.tm_mon  = random(0, max_month); // months are zero-based
        date.tm_mday = random(1, max_day);
        date.tm_hour = hour;

        // add the difference in seconds to the start
        constexpr int max_seconds = 3 * 60 * 60;

        const seconds                  diff{static_cast<seconds::rep>(random(0, max_seconds))};
        const system_clock::time_point end = system_clock::from_time_t(std::mktime(&date));
        const system_clock::time_point start{end.time_since_epoch() - diff};
        val.times.emplace_back(start, end);
    }
    return result;
}

TEST_CASE("element addition") {
    apptime::database db{test_paths.ele_add};

    SECTION("add an active element") {
        const apptime::record &element_test = processes.front();
        REQUIRE(db.add_active(element_test));
    }

    SECTION("add a focus element") {
        const apptime::record &element_test = processes.front();
        REQUIRE(db.add_focus(element_test));
    }

    SECTION("ignore file") {
        const apptime::record &element_test = *(processes.begin() + 1);
        db.add_ignore(apptime::ignore_file, element_test.path);
        REQUIRE_FALSE(db.add_active(element_test));
        REQUIRE_FALSE(db.add_focus(element_test));

        db.remove_ignore(apptime::ignore_file, element_test.path);
        REQUIRE(db.add_active(element_test));
        REQUIRE(db.add_focus(element_test));
    }

    SECTION("ignore path") {
        const apptime::record &element_test = *(processes.begin() + 2);
        std::string            path         = fs::path{element_test.path}.parent_path().string();
        db.add_ignore(apptime::ignore_path, element_test.path);
        REQUIRE_FALSE(db.add_active(element_test));
        REQUIRE_FALSE(db.add_focus(element_test));

        db.remove_ignore(apptime::ignore_path, element_test.path);
        REQUIRE(db.add_active(element_test));
        REQUIRE(db.add_focus(element_test));
    }
}

TEST_CASE("element search") {
    apptime::database db{test_paths.ele_search};

    SECTION("add all elements") {
        for (const auto &val: processes) {
            REQUIRE(db.add_active(val));
        }
        REQUIRE(db.actives().size() == processes.size());
    }

    SECTION("search by year") {
        const auto &test_time = processes.front().times.front().first;
        const auto  ymd       = year_month_day{floor<days>(test_time)};
        const auto  year      = ymd.year();

        const std::ptrdiff_t assumed = std::ranges::count_if(processes, [year](const apptime::record &ele) {
            return std::ranges::any_of(ele.times, [year](const auto &time) {
                const year_month_day start{floor<days>(time.first)};
                const year_month_day end{floor<days>(time.second)};
                return start.year() == year || end.year() == year;
            });
        });

        apptime::database::options opt;
        opt.date = year;
        REQUIRE(db.actives(opt).size() == assumed);
    }

    SECTION("search by year/month") {
        const auto &test_time = (processes.begin() + 1)->times.front().first;
        const auto  ymd       = year_month_day{floor<days>(test_time)};
        const auto  ym        = ymd.year() / ymd.month();

        const std::ptrdiff_t assumed = std::ranges::count_if(processes, [ym](const apptime::record &ele) {
            return std::ranges::any_of(ele.times, [ym](const auto &time) {
                const year_month_day start{floor<days>(time.first)};
                const year_month_day end{floor<days>(time.second)};

                const auto ym_start = start.year() / start.month();
                const auto ym_end   = end.year() / end.month();
                return ym_start == ym || ym_end == ym;
            });
        });

        apptime::database::options opt;
        opt.date = ym;
        REQUIRE(db.actives(opt).size() == assumed);
    }

    SECTION("search by year/month/day") {
        const auto &test_time = (processes.begin() + 2)->times.front().first;
        const auto  ymd       = year_month_day{floor<days>(test_time)};

        const std::ptrdiff_t assumed = std::ranges::count_if(processes, [ymd](const apptime::record &ele) {
            return std::ranges::any_of(ele.times, [ymd](const auto &time) {
                const year_month_day start{floor<days>(time.first)};
                const year_month_day end{floor<days>(time.second)};
                return start == ymd || end == ymd;
            });
        });

        apptime::database::options opt;
        opt.date = ymd;
        REQUIRE(db.actives(opt).size() == assumed);
    }

    SECTION("search by path") {
        const apptime::record &element_test = *(processes.begin() + 3);

        apptime::database::options opt;
        opt.path = element_test.path;
        REQUIRE(db.actives(opt).size() == 1); // it's assumed that all activities of the application will be in one structure
    }
}

TEST_CASE("cleanup") {
    REQUIRE_NOTHROW(std::filesystem::remove(test_paths.ele_add));
    REQUIRE_NOTHROW(std::filesystem::remove(test_paths.ele_search));
}

int main(int argc, char *argv[]) {
    constexpr int max_processes = 15;
    processes                   = random_processes(max_processes); // generate random "processes" to test the class
    return Catch::Session().run(argc, argv);
}