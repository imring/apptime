#ifndef APPTIME_ICON_HPP
#define APPTIME_ICON_HPP

#include <QIcon>

#include <string_view>

namespace apptime {
QIcon application_icon(std::string_view path);
} // namespace apptime

#endif // APPTIME_ICON_HPP