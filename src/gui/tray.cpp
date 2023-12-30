#include "tray.hpp"
#include "window.hpp"

#include <QMenu>
#include <QApplication>

QString status_text(bool running) {
    return running ? "Stop" : "Start";
}

namespace apptime {
tray::tray(QObject *parent) : QSystemTrayIcon{parent}, menu_{new QMenu{}} {
    const auto  win = qobject_cast<window *>(parent);
    const QIcon icon{"./icon.png"};

    setIcon(icon);
    setContextMenu(menu_);

    // Open the window by clicking on the icon

    connect(this, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            qobject_cast<window *>(this->parent())->show();
        }
    });

    // Toggle (Start/Stop)

    QAction *toggle_action = new QAction{status_text(win->running()), this};
    menu_->addAction(toggle_action);
    menu_->addSeparator();
    connect(toggle_action, &QAction::triggered, [this, toggle_action]() {
        auto win = qobject_cast<window *>(this->parent());
        win->toggle();
        toggle_action->setText(status_text(win->running()));
    });

    // Close

    QAction *close_action = new QAction{"Close", this};
    menu_->addAction(close_action);
    connect(close_action, &QAction::triggered, &QCoreApplication::quit);
}
} // namespace apptime