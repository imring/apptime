#include <QApplication>

#include "gui/tray.hpp"

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("imring");
    QCoreApplication::setOrganizationDomain("imring.dev");
    QCoreApplication::setApplicationName("apptime");

    QApplication  app{argc, argv};
    apptime::tray tray_icon{&app};

    tray_icon.show();
    return app.exec();
}