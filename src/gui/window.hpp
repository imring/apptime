#ifndef APPTIME_GUI_WINDOW_HPP
#define APPTIME_GUI_WINDOW_HPP

#include <QMainWindow>

#include "../monitoring.hpp"

namespace apptime {
class window : public QMainWindow {
    Q_OBJECT
public:
    window(QWidget *parent = nullptr);

    void closeEvent(QCloseEvent *event) override;

    void toggle();
    bool running() const {
        return monitor_.running();
    }

private:
    database   db_;
    monitoring monitor_;
};
} // namespace apptime

#endif // APPTIME_GUI_WINDOW_HPP