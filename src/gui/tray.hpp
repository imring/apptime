#ifndef APPTIME_GUI_TRAY_HPP
#define APPTIME_GUI_TRAY_HPP

#include <QSystemTrayIcon>

namespace apptime {
class tray : public QSystemTrayIcon {
    Q_OBJECT
public:
    tray(QObject *parent = nullptr);

private:
    QMenu *menu_;
};
} // namespace apptime

#endif // APPTIME_GUI_TRAY_HPP