#include "window.hpp"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QSettings>
#include <QTableView>
#include <QVBoxLayout>

#include "database/database_sqlite.hpp"
#include "ignore.hpp"

namespace apptime {
window::window(QWidget *parent) : QMainWindow{parent}, db_{std::make_shared<database_sqlite>("./result.db")}, monitor_{db_} {
    const QIcon icon{":/icon.png"};
    setWindowIcon(icon);

    // interface setup
    auto *central_widget = new QWidget{this};
    setCentralWidget(central_widget);
    auto *vbox = new QVBoxLayout;
    central_widget->setLayout(vbox);

    addOptionalWidgets();
    addResults();
    addMenubar();

    // initial update
    settings_window_ = new settings_window{this}; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    settings_window_->readSettings();
    updateRecords();

    // monitor launch
    monitor_.start();

    // add slots
    connect(table_widget_, &table_records::addIgnore, this, &window::addIgnore);
}

void window::closeEvent(QCloseEvent *event) {
    if (event->spontaneous()) { // close by user
        event->ignore();
        hide();
        return;
    }

    // close by program
    settings_window_->saveSettings();
}

void window::toggle() {
    if (monitor_.running()) {
        monitor_.stop();
    } else {
        monitor_.start();
    }
}

void window::addMenubar() {
    // File
    QMenu *file_menu       = menuBar()->addMenu(tr("&File"));
    auto  *settings_action = new QAction{"&Settings", this};
    file_menu->addAction(settings_action);
    auto *close_action = new QAction{"&Close", this};
    file_menu->addAction(close_action);

    // View
    QMenu *view_menu = menuBar()->addMenu(tr("&View"));

    // _ Focused only
    auto *toggle_focus = new QAction{"&Focused only", this};
    toggle_focus->setCheckable(true);
    toggle_focus->setChecked(focus_widget_->checkState() != Qt::Unchecked);
    view_menu->addAction(toggle_focus);

    // _ Show window names
    toggle_names_ = new QAction{"&Show window names", this};
    toggle_names_->setCheckable(true);
    view_menu->addAction(toggle_names_);

    // _ Show application icons
    toggle_icons_ = new QAction{"&Show application icons", this};
    toggle_icons_->setCheckable(true);
    view_menu->addAction(toggle_icons_);

    // Ignore list...
    view_menu->addSeparator();
    auto *ignore_list = new QAction{"&Ignore list...", this};
    view_menu->addAction(ignore_list);

    // slots
    connect(settings_action, &QAction::triggered, [this] {
        settings_window_->show();
    });
    connect(close_action, &QAction::triggered, this, &QWidget::close);
    connect(toggle_focus, &QAction::triggered, focus_widget_, &QAbstractButton::toggle);
    connect(focus_widget_, &QCheckBox::stateChanged, [toggle_focus](int state) {
        toggle_focus->setChecked(state != Qt::Unchecked);
    });
    connect(ignore_list, &QAction::triggered, this, &window::openIgnoreWindow);
    connect(toggle_names_, &QAction::triggered, this, &window::updateRecords);
    connect(toggle_icons_, &QAction::triggered, this, &window::updateRecords);
}

void window::addOptionalWidgets() {
    auto *main_layout   = qobject_cast<QVBoxLayout *>(centralWidget()->layout());
    auto *option_layout = new QFormLayout;

    // Filter by: ____
    filter_widget_ = new QComboBox;
    filter_widget_->addItems({"Day", "Month", "Year", "All"});
    option_layout->addRow("Filter by:", filter_widget_);

    // Select a date: ____
    date_widget_ = new QDateEdit{QDate::currentDate()};
    option_layout->addRow("Select a date:", date_widget_);

    // _ Only focused processes
    focus_widget_ = new QCheckBox{"Only focused processes"};
    option_layout->addRow(focus_widget_);

    main_layout->addLayout(option_layout);

    connect(filter_widget_, &QComboBox::currentIndexChanged, this, &window::updateFormat);
    connect(date_widget_, &QDateEdit::userDateChanged, this, &window::updateRecords);
    connect(focus_widget_, &QCheckBox::stateChanged, this, &window::updateRecords);
}

void window::addResults() {
    table_widget_ = new table_records;
    centralWidget()->layout()->addWidget(table_widget_);
}

void window::updateRecords() {
    const QDate date    = date_widget_->date();
    const bool  focused = focus_widget_->checkState() != Qt::Unchecked;

    apptime::database::options opt;
    switch (static_cast<DateFormat>(filter_widget_->currentIndex())) {
    case DateFormat::Year:
        opt.date = std::chrono::year{date.year()};
        break;
    case DateFormat::Month:
        opt.date = std::chrono::year{date.year()} / date.month();
        break;
    case DateFormat::Day:
        opt.date = std::chrono::year{date.year()} / date.month() / date.day();
        break;
    default:
        break;
    }

    std::vector<apptime::record> records;
    if (focused) {
        records = db_->focuses(opt);
    } else {
        records = db_->actives(opt);
    }

    table_records::settings settings = {};
    settings.window_names            = toggle_names_->isChecked();
    settings.icons                   = toggle_icons_->isChecked();
    table_widget_->update(records, settings);
}

QString window::getDateFormat(DateFormat format) {
    QString result;
    switch (format) {
    case DateFormat::Day:
        result = "dd.MM.yyyy";
        break;
    case DateFormat::Month:
        result = "MM.yyyy";
        break;
    case DateFormat::Year:
        result = "yyyy";
        break;
    case DateFormat::All:
        result = "";
        break;
    default:
        break;
    }
    return result;
}

window::DateFormat window::getDateFormat(QAnyStringView format) {
    if (format == "dd.MM.yyyy") {
        return DateFormat::Day;
    }
    if (format == "MM.yyyy") {
        return DateFormat::Month;
    }
    if (format == "yyyy") {
        return DateFormat::Year;
    }
    return DateFormat::All;
}

// signals

void window::updateFormat(int index) {
    const auto format = static_cast<DateFormat>(index);
    date_widget_->setDisplayFormat(getDateFormat(format));
    updateRecords();
}

void window::openIgnoreWindow() {
    if (!ignore_window_) {
        ignore_window_ = new ignore_window{this};
        connect(ignore_window_, &ignore_window::needToAdd, this, &window::addIgnore);
        connect(ignore_window_, &ignore_window::needToRemove, this, &window::removeIgnore);
    }
    ignore_window_->update(db_->ignores());
    ignore_window_->show();
}

void window::addIgnore(ignore_type type, std::string_view path) {
    db_->add_ignore(type, path);
    updateRecords();
    if (ignore_window_) {
        ignore_window_->update(db_->ignores());
    }
}

void window::removeIgnore(ignore_type type, std::string_view path) {
    db_->remove_ignore(type, path);
    updateRecords();
    if (ignore_window_) {
        ignore_window_->update(db_->ignores());
    }
}
} // namespace apptime