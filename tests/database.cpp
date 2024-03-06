#include <chrono>
#include <filesystem>

#include <sqlite3.h>
#include <SQLiteCpp/SQLiteCpp.h>

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
        auto &v = *it;

        // random path generating (e.g. /foo/bar)
        do {
            int subfolders = 2; // 1 subfolders + 1 file
            while (subfolders--) {
                v.path += "/" + random_string(random(1, 10));
            }
        } while (!path_is_unique(v.path, result.begin(), it));

        // random name generating
        v.name = random_string(random(1, 10));

        // random time generating
        std::tm date = {};
        date.tm_year = random(2021, 2023) - 1900; // years since 1900
        date.tm_mon  = random(0, 11);             // months are zero-based
        date.tm_mday = random(1, 28);
        date.tm_hour = 12;

        const seconds                  diff{static_cast<seconds::rep>(random(0, 3 * 60 * 60))}; // from 0 seconds to 3 hours in seconds
        const system_clock::time_point end = system_clock::from_time_t(std::mktime(&date));
        const system_clock::time_point start{end.time_since_epoch() - diff};
        v.times.emplace_back(start, end);
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
        for (const auto &v: processes) {
            REQUIRE(db.add_active(v));
        }
        REQUIRE(db.actives().size() == processes.size());
    }

    SECTION("search by year") {
        const auto &test_time = processes.front().times.front().first;
        const auto  ymd       = year_month_day{floor<days>(test_time)};
        const auto  y         = ymd.year();

        const std::ptrdiff_t assumed = std::count_if(processes.begin(), processes.end(), [y](const apptime::record &ele) {
            for (const auto &[start, end]: ele.times) {
                const auto y_start = year_month_day{floor<days>(start)}, y_end = year_month_day{floor<days>(end)};
                if (y_start.year() == y || y_end.year() == y) {
                    return true;
                }
            }
            return false;
        });

        apptime::database::options opt;
        opt.year = static_cast<int>(y);
        REQUIRE(db.actives(opt).size() == assumed);
    }

    SECTION("search by year/month") {
        const auto &test_time = (processes.begin() + 1)->times.front().first;
        const auto  ymd       = year_month_day{floor<days>(test_time)};
        const auto  ym        = ymd.year() / ymd.month();

        const std::ptrdiff_t assumed = std::count_if(processes.begin(), processes.end(), [ym](const apptime::record &ele) {
            for (const auto &[start, end]: ele.times) {
                const auto ymd_start = year_month_day{floor<days>(start)}, ymd_end = year_month_day{floor<days>(end)};
                const auto ym_start = ymd_start.year() / ymd_start.month(), ym_end = ymd_end.year() / ymd_end.month();
                if (ym_start == ym || ym_end == ym) {
                    return true;
                }
            }
            return false;
        });

        apptime::database::options opt;
        opt.year  = static_cast<int>(ymd.year());
        opt.month = static_cast<unsigned>(ymd.month());
        REQUIRE(db.actives(opt).size() == assumed);
    }

    SECTION("search by year/month/day") {
        const auto &test_time = (processes.begin() + 2)->times.front().first;
        const auto  ymd       = year_month_day{floor<days>(test_time)};

        const std::ptrdiff_t assumed = std::count_if(processes.begin(), processes.end(), [ymd](const apptime::record &ele) {
            for (const auto &[start, end]: ele.times) {
                const auto ymd_start = year_month_day{floor<days>(start)}, ymd_end = year_month_day{floor<days>(end)};
                if (ymd_start == ymd || ymd_end == ymd) {
                    return true;
                }
            }
            return false;
        });

        apptime::database::options opt;
        opt.year  = static_cast<int>(ymd.year());
        opt.month = static_cast<unsigned>(ymd.month());
        opt.day   = static_cast<unsigned>(ymd.day());
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
    processes = random_processes(15); // generate 15 random "processes" to test the class
    return Catch::Session().run(argc, argv);
}