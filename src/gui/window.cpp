#include "window.hpp"

#include <QFrame>
#include <QLabel>
#include <QMenuBar>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QCoreApplication>

#include "ignore.hpp"

namespace apptime {
window::window(QWidget *parent) : QMainWindow{parent}, db_{"./result.db"}, monitor_{db_} {
    const QIcon icon{"./icon.png"};
    setWindowIcon(icon);

    // interface setup
    QWidget *wid = new QWidget{this};
    setCentralWidget(wid);
    QVBoxLayout *vbox = new QVBoxLayout;
    wid->setLayout(vbox);

    addOptionalWidgets();
    addSeparator();
    addResults();
    addMenubar();

    // initial update
    updateFormat(0);

    // monitor launch
    monitor_.start();

    // add slots
    connect(table_widget_, &records::addIgnore, this, &window::addIgnore);
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

void window::addMenubar() {
    // File
    QMenu   *file_menu    = menuBar()->addMenu(tr("&File"));
    QAction *close_action = new QAction{"&Close", this};
    file_menu->addAction(close_action);

    // View
    QMenu   *view_menu    = menuBar()->addMenu(tr("&View"));
    QAction *toggle_focus = new QAction{"&Focused only", this};
    toggle_focus->setCheckable(true);
    toggle_focus->setChecked(focus_widget_->checkState() != Qt::Unchecked);
    view_menu->addAction(toggle_focus);
    view_menu->addSeparator();
    QAction *ignore_list = new QAction{"&Ignore list...", this};
    view_menu->addAction(ignore_list);

    // slots
    connect(close_action, &QAction::triggered, &QCoreApplication::exit);
    connect(toggle_focus, &QAction::triggered, focus_widget_, &QAbstractButton::toggle);
    connect(focus_widget_, &QCheckBox::stateChanged, [toggle_focus](int state) {
        toggle_focus->setChecked(state != Qt::Unchecked);
    });
    connect(ignore_list, &QAction::triggered, [this]() {
        if (!ignore_window_) {
            ignore_window_ = new ignore_window{this};
            connect(ignore_window_, &ignore_window::needToAdd, this, &window::addIgnore);
            connect(ignore_window_, &ignore_window::needToRemove, this, &window::removeIgnore);
        }
        ignore_window_->update(db_.ignores());
        ignore_window_->show();
    });
}

void window::addOptionalWidgets() {
    const auto main_layout = static_cast<QVBoxLayout *>(centralWidget()->layout());

    // Filter by: ____
    {
        QHBoxLayout *hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel{"Filter by:"});
        filter_widget_ = new QComboBox;
        filter_widget_->addItems({"Day", "Month", "Year", "All"});
        hbox->addWidget(filter_widget_);

        main_layout->addLayout(hbox);
    }

    // Select a date: ____
    {
        QHBoxLayout *hbox = new QHBoxLayout;
        hbox->addWidget(new QLabel{"Select a date:"});
        date_widget_ = new QDateEdit{QDate::currentDate()};
        hbox->addWidget(date_widget_);

        main_layout->addLayout(hbox);
    }

    // _ Only focused processes
    focus_widget_ = new QCheckBox{"Only focused processes"};
    main_layout->addWidget(focus_widget_);

    connect(filter_widget_, &QComboBox::currentIndexChanged, this, &window::updateFormat);
    connect(date_widget_, &QDateEdit::userDateChanged, this, &window::updateDate);
    connect(focus_widget_, &QCheckBox::stateChanged, this, &window::updateFocused);
}

void window::addSeparator() {
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    centralWidget()->layout()->addWidget(line);
}

void window::addResults() {
    table_widget_ = new records;
    centralWidget()->layout()->addWidget(table_widget_);
}

// signals

void window::updateDate(const QDate &date) {
    std::vector<apptime::record> records;
    const bool                   focused = focus_widget_->checkState() != Qt::Unchecked;
    apptime::database::options   opt;
    switch (filter_widget_->currentIndex()) {
    case 0:
        opt.day = date.day();
        [[fallthrough]];
    case 1:
        opt.month = date.month();
        [[fallthrough]];
    case 2:
        opt.year = date.year();
        break;
    default:
        break;
    }

    if (focused) {
        records = db_.focuses(opt);
    } else {
        records = db_.actives(opt);
    }
    table_widget_->update(records);
}

void window::updateFormat(int index) {
    switch (index) {
    case 0:
        date_widget_->setDisplayFormat("dd.MM.yyyy");
        break;
    case 1:
        date_widget_->setDisplayFormat("MM.yyyy");
        break;
    case 2:
        date_widget_->setDisplayFormat("yyyy");
        break;
    case 3:
        date_widget_->setDisplayFormat("");
        break;
    default:
        break;
    }
    updateDate(date_widget_->date());
}

void window::updateFocused(int state) {
    updateDate(date_widget_->date());
}

void window::addIgnore(ignore_type type, std::string_view path) {
    db_.add_ignore(type, path);
    updateDate(date_widget_->date());
    if (ignore_window_) {
        ignore_window_->update(db_.ignores());
    }
}

void window::removeIgnore(ignore_type type, std::string_view path) {
    db_.remove_ignore(type, path);
    updateDate(date_widget_->date());
    if (ignore_window_) {
        ignore_window_->update(db_.ignores());
    }
}
} // namespace apptime