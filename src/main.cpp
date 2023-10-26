#include <chrono>
#include <iostream>
#include <algorithm>
#include <filesystem>

#include <Windows.h>

#include "process.hpp"
#include "database.hpp"

std::vector<apptime::process> processes() {
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

void process_test() {
    for (const auto &v: processes()) {
        std::cout << v.full_path() << " " << v.start() << " " << v.window_name() << std::endl;
    }
    const auto focused = apptime::process::focused_window();
    std::cout << std::endl << "Focused: " << focused.full_path() << " " << focused.start() << " " << focused.window_name() << std::endl;
}

void database_test() {
    apptime::database db{"./result.db"};
    db.add_ignore(apptime::ignore_file, "C:/Windows/explorer.exe");
    db.add_ignore(apptime::ignore_path, "C:/Windows/System32");
    db.add_focus(apptime::process::focused_window());

    for (const auto &v: processes()) {
        db.add_active(v);
    }
    for (const auto &v: db.actives()) {
        std::cout << v.name << " (" << v.path << "):" << std::endl;
        for (const auto &[start, end]: v.times) {
            std::cout << "- " << start << ' ' << end << std::endl;
        }
    }
}

int main() {
    process_test();
    database_test();

    return 0;
}