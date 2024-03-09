#ifndef TESTS_UTILS_HPP
#define TESTS_UTILS_HPP

#include <random>
#include <type_traits>

template <typename T>
    requires(std::is_arithmetic_v<T>)
T random(T min, T max) {
    static std::random_device rd;
    static std::mt19937       gen{rd()};

    if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> distrib{min, max};
        return distrib(gen);
    } else {
        std::uniform_int_distribution<T> distrib{min, max};
        return distrib(gen);
    }
}

inline std::string random_string(int size) {
    constexpr std::string_view alphanum = "0123456789"
                                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                          "abcdefghijklmnopqrstuvwxyz";

    std::string result;
    result.reserve(size);
    while (size--) {
        result.push_back(alphanum[random<std::size_t>(0, alphanum.size() - 1)]);
    }
    return result;
}

#endif // TESTS_UTILS_HPP