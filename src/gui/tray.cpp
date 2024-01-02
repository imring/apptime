#include "tray.hpp"
#include "window.hpp"

#include <QApplication>
#include <QMenu>

QString status_text(bool running) {
    return running ? QStringLiteral("Stop") : QStringLiteral("Start");
}

namespace apptime {
tray::tray(QObject *parent) : QSystemTrayIcon{parent}, menu_{new QMenu{}} {
    const auto *win = qobject_cast<window *>(parent);
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
    auto *toggle_action = new QAction{status_text(win->running()), this};
    menu_->addAction(toggle_action);
    menu_->addSeparator();
    connect(toggle_action, &QAction::triggered, [this, toggle_action]() {
        auto *win = qobject_cast<window *>(this->parent());
        win->toggle();
        toggle_action->setText(status_text(win->running()));
    });

    // Close
    auto *close_action = new QAction{QStringLiteral("Close"), this};
    menu_->addAction(close_action);
    connect(close_action, &QAction::triggered, [this]() {
        qobject_cast<window *>(this->parent())->close();
    });
}
} // namespace apptime