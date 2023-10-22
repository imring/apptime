#include <chrono>
#include <iostream>
#include <algorithm>

#include <Windows.h>

#include "process.hpp"

int main() {
    auto result = apptime::process::active_windows();

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

    for (const auto &v: result) {
        std::cout << v.full_path() << " " << v.start() << " " << v.window_name() << std::endl;
    }
    const auto focused = apptime::process::focused_window();
    std::cout << std::endl << "Focused: " << focused.full_path() << " " << focused.start() << " " << focused.window_name() << std::endl;

    return 0;
}