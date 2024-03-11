#ifndef APPTIME_ENCODING_HPP
#define APPTIME_ENCODING_HPP

#include <string_view>

namespace apptime {
#if defined(WIN32) && defined(UNICODE)
using tstring = std::wstring;

std::string  utf8_encode(std::wstring_view wstr);
std::wstring utf8_decode(std::string_view str);
#else
using tstring = std::string;

inline std::string utf8_encode(std::string_view str) {
    return std::string{str};
}

inline std::string utf8_decode(std::string_view str) {
    return std::string{str};
}
#endif
} // namespace apptime

#endif // APPTIME_ENCODING_HPP