#ifndef APPTIME_GUI_SETTINGS_HPP
#define APPTIME_GUI_SETTINGS_HPP

#include <QListWidget>
#include <QSpinBox>
#include <QStackedWidget>
#include <QWidget>

namespace apptime {
class settings_window : public QWidget {
    Q_OBJECT

public:
    explicit settings_window(QWidget *parent = nullptr);

    void readSettings();
    void saveSettings();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void initMonitoringSettings();

    QSpinBox *active_delay_ = nullptr;
    QSpinBox *focus_delay_  = nullptr;

    QListWidget    *listbox_ = nullptr;
    QStackedWidget *widgets_ = nullptr;
};
} // namespace apptime

#endif // APPTIME_GUI_SETTINGS_HPP