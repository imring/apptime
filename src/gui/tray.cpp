#include "tray.hpp"

#include <QMenu>
#include <QApplication>

namespace apptime {
tray::tray(QObject *parent) : QSystemTrayIcon{parent}, menu_{new QMenu{}} {
    const QIcon icon{"./icon.png"};

    setIcon(icon);
    setContextMenu(menu_);

    QAction *close_action = new QAction{"Close", this};
    menu_->addAction(close_action);

    connect(close_action, &QAction::triggered, &QCoreApplication::quit);
}
} // namespace apptime