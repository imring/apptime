#include "settings.hpp"
#include "window.hpp"

#include <QAction>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QSettings>

namespace apptime {
settings_window::settings_window(QWidget *parent) : QWidget{parent}, listbox_{new QListWidget}, widgets_{new QStackedWidget} {
    setWindowFlag(Qt::Window);

    // widgets
    initMonitoringSettings();

    auto *hbox = new QHBoxLayout{this};
    hbox->addWidget(listbox_, 0);
    hbox->addWidget(widgets_, 1);
    setLayout(hbox);

    readSettings();

    connect(listbox_, &QListWidget::currentRowChanged, widgets_, &QStackedWidget::setCurrentIndex);
}

void settings_window::closeEvent(QCloseEvent * /*event*/) {
    saveSettings();
    readSettings();
}

void settings_window::readSettings() {
    auto *window_ptr = qobject_cast<apptime::window *>(parent());
    if (!window_ptr) {
        return;
    }
    QSettings settings;

    // window
    settings.beginGroup("window");
    const auto geometry = settings.value("geometry", QByteArray{}).toByteArray();
    if (!geometry.isEmpty()) {
        window_ptr->restoreGeometry(geometry);
    }
    settings.endGroup();

    // view
    settings.beginGroup("view");
    const auto focused_only = settings.value("focused_only", false).toBool();
    window_ptr->focus_widget_->setCheckState(focused_only ? Qt::Checked : Qt::Unchecked);

    const auto window_names = settings.value("window_names", true).toBool();
    window_ptr->toggle_names_->setChecked(window_names);

    const auto date_format = settings.value("date_format", static_cast<int>(window::DateFormat::Day)).toInt();
    window_ptr->date_widget_->setDisplayFormat(window::getDateFormat(static_cast<window::DateFormat>(date_format)));

    const auto show_icons = settings.value("show_icons", true).toBool();
    window_ptr->toggle_icons_->setChecked(show_icons);
    settings.endGroup();

    // monitoring
    settings.beginGroup("monitoring");
    const auto active_delay           = settings.value("active_delay", 5000).toInt();
    window_ptr->monitor_.active_delay = std::chrono::milliseconds{active_delay};
    active_delay_->setValue(active_delay);

    const auto focus_delay           = settings.value("focus_delay", 5000).toInt();
    window_ptr->monitor_.focus_delay = std::chrono::milliseconds{focus_delay};
    focus_delay_->setValue(focus_delay);
    settings.endGroup();
}

void settings_window::saveSettings() {
    auto *window_ptr = qobject_cast<apptime::window *>(parent());
    if (!window_ptr) {
        return;
    }
    QSettings settings;

    // window
    settings.beginGroup("window");
    settings.setValue("geometry", window_ptr->saveGeometry());
    settings.endGroup();

    // view
    settings.beginGroup("view");
    settings.setValue("focused_only", window_ptr->focus_widget_->checkState() == Qt::Checked);
    settings.setValue("window_names", window_ptr->toggle_names_->isChecked());
    settings.setValue("date_format", static_cast<int>(window::getDateFormat(window_ptr->date_widget_->displayFormat())));
    settings.setValue("show_icons", window_ptr->toggle_icons_->isChecked());
    settings.endGroup();

    // monitoring
    settings.beginGroup("monitoring");
    settings.setValue("active_delay", active_delay_->value());
    settings.setValue("focus_delay", focus_delay_->value());
    settings.endGroup();
}

void settings_window::initMonitoringSettings() {
    auto *widget = new QWidget;
    auto *layout = new QFormLayout;

    active_delay_ = new QSpinBox;
    active_delay_->setMinimum(1);
    active_delay_->setMaximum(std::numeric_limits<int>::max());
    layout->addRow(QStringLiteral("Scan active windows every N milliseconds: "), active_delay_);

    focus_delay_ = new QSpinBox;
    focus_delay_->setMinimum(1);
    focus_delay_->setMaximum(std::numeric_limits<int>::max());
    layout->addRow(QStringLiteral("Scan focused windows every N milliseconds: "), focus_delay_);

    widget->setLayout(layout);

    listbox_->addItem(QStringLiteral("Monitoring"));
    widgets_->addWidget(widget);
}
} // namespace apptime