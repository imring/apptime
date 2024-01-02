#include "icon.hpp"
#include "encoding.hpp"

#include <Windows.h>

namespace apptime {
QIcon application_icon(std::string_view path) {
    if (path.empty()) {
        return {};
    }
    const tstring unicode_path = utf8_decode(path);
    HICON         icon         = ExtractIcon(nullptr, unicode_path.data(), 0);
    if (!icon) {
        return {};
    }

    const QPixmap pixmap = QPixmap::fromImage(QImage::fromHICON(icon));
    DestroyIcon(icon);
    return pixmap;
}
} // namespace apptime