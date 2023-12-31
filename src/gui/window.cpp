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

#include "ignore.hpp"

namespace apptime {
window::window(QWidget *parent) : QMainWindow{parent}, db_{"./result.db"}, monitor_{db_} {
    const QIcon icon{":/icon.png"};
    setWindowIcon(icon);

    // interface setup
    auto *central_widget = new QWidget{this};
    setCentralWidget(central_widget);
    auto *vbox = new QVBoxLayout;
    central_widget->setLayout(vbox);

    addOptionalWidgets();
    addSeparator();
    addResults();
    addMenubar();

    // initial update
    readSettings();
    updateRecords();

    // monitor launch
    monitor_.start();

    // add slots
    connect(table_widget_, &records::addIgnore, this, &window::addIgnore);
}

void window::closeEvent(QCloseEvent *event) {
    if (event->spontaneous()) { // close by user
        event->ignore();
        hide();
        return;
    }

    // close by program
    saveSettings();
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
    QMenu *file_menu    = menuBar()->addMenu(tr("&File"));
    auto  *close_action = new QAction{"&Close", this};
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

void window::addSeparator() {
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    centralWidget()->layout()->addWidget(line);
}

void window::addResults() {
    table_widget_ = new records;
    centralWidget()->layout()->addWidget(table_widget_);
}

void window::updateRecords() {
    const QDate date    = date_widget_->date();
    const bool  focused = focus_widget_->checkState() != Qt::Unchecked;

    apptime::database::options opt;
    switch (static_cast<DateFormat>(filter_widget_->currentIndex())) {
    case DateFormat::Day:
        opt.day = date.day();
        [[fallthrough]];
    case DateFormat::Month:
        opt.month = date.month();
        [[fallthrough]];
    case DateFormat::Year:
        opt.year = date.year();
        break;
    default:
        break;
    }

    std::vector<apptime::record> records;
    if (focused) {
        records = db_.focuses(opt);
    } else {
        records = db_.actives(opt);
    }

    records::settings settings = {};
    settings.window_names      = toggle_names_->isChecked();
    settings.icons             = toggle_icons_->isChecked();
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

void window::readSettings() {
    QSettings settings;

    // window
    settings.beginGroup("window");
    const auto geometry = settings.value("geometry", QByteArray{}).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    settings.endGroup();

    // view
    settings.beginGroup("view");
    const auto focused_only = settings.value("focused_only", false).toBool();
    focus_widget_->setCheckState(focused_only ? Qt::Checked : Qt::Unchecked);

    const auto window_names = settings.value("window_names", true).toBool();
    toggle_names_->setChecked(window_names);

    const auto date_format = settings.value("date_format", static_cast<int>(DateFormat::Day)).toInt();
    date_widget_->setDisplayFormat(getDateFormat(static_cast<DateFormat>(date_format)));

    const auto show_icons = settings.value("show_icons", true).toBool();
    toggle_icons_->setChecked(show_icons);
    settings.endGroup();
}

void window::saveSettings() {
    QSettings settings;

    // window
    settings.beginGroup("window");
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();

    // view
    settings.beginGroup("view");
    settings.setValue("focused_only", focus_widget_->checkState() == Qt::Checked);
    settings.setValue("window_names", toggle_names_->isChecked());
    settings.setValue("date_format", static_cast<int>(getDateFormat(date_widget_->displayFormat())));
    settings.setValue("show_icons", toggle_icons_->isChecked());
    settings.endGroup();
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
    ignore_window_->update(db_.ignores());
    ignore_window_->show();
}

void window::addIgnore(ignore_type type, std::string_view path) {
    db_.add_ignore(type, path);
    updateRecords();
    if (ignore_window_) {
        ignore_window_->update(db_.ignores());
    }
}

void window::removeIgnore(ignore_type type, std::string_view path) {
    db_.remove_ignore(type, path);
    updateRecords();
    if (ignore_window_) {
        ignore_window_->update(db_.ignores());
    }
}
} // namespace apptime