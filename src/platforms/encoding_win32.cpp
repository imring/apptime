#include "encoding.hpp"

#include <Windows.h>

namespace apptime {
#if defined(WIN32) && defined(UNICODE)
// https://stackoverflow.com/a/3999597
std::string utf8_encode(std::wstring_view wstr) {
    if (wstr.empty()) {
        return {};
    }
    const int   size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

std::wstring utf8_decode(std::string_view str) {
    if (str.empty()) {
        return {};
    }
    const int    size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(size_needed, '\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), size_needed);
    return result;
}
#endif
} // namespace apptime