#ifndef APPTIME_GUI_WINDOW_HPP
#define APPTIME_GUI_WINDOW_HPP

#include <QDateEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QMainWindow>

#include "../monitoring.hpp"
#include "table.hpp"

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

public slots:
    void updateDate(const QDate &date);
    void updateFormat(int index);
    void updateFocused(int state);

private:
    void addOptionalWidgets();
    void addSeparator();
    void addResults();

private:
    QCheckBox *focus_widget_  = nullptr;
    QComboBox *filter_widget_ = nullptr;
    QDateEdit *date_widget_   = nullptr;
    table     *table_widget_  = nullptr;

    database   db_;
    monitoring monitor_;
};
} // namespace apptime

#endif // APPTIME_GUI_WINDOW_HPP