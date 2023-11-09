#include "window.hpp"

#include <QCloseEvent>

namespace apptime {
window::window(QWidget *parent) : QMainWindow{parent}, db_{"./result.db"}, monitor_{db_} {
    const QIcon icon{"./icon.png"};
    setWindowIcon(icon);

    monitor_.start();
}

void window::closeEvent(QCloseEvent *event) {
    event->ignore();
    hide();
}

void window::toggle() {
    if (monitor_.running()) {
        monitor_.stop();
    } else {
        monitor_.start();
    }
}
} // namespace apptime