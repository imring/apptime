#include <QApplication>

#include "gui/tray.hpp"
#include "gui/window.hpp"

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("imring");
    QCoreApplication::setOrganizationDomain("imring.dev");
    QCoreApplication::setApplicationName("apptime");

    const QApplication app{argc, argv};
    apptime::window    window;
    apptime::tray      tray_icon{&window};

    window.show();
    tray_icon.show();
    return QApplication::exec();
}