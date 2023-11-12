#include "window.hpp"

#include <QFrame>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>

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

    // initial update
    updateFormat(0);

    // monitor launch
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
    table_widget_ = new table;
    centralWidget()->layout()->addWidget(table_widget_);
}

// signals

void window::updateDate(const QDate &date) {
    std::vector<apptime::active> actives;
    const bool                   focused = focus_widget_->checkState() != Qt::Unchecked;
    switch (filter_widget_->currentIndex()) {
    case 0:
        actives = updateWithFocused(focused, date.year(), date.month(), date.day());
        break;
    case 1:
        actives = updateWithFocused(focused, date.year(), date.month());
        break;
    case 2:
        actives = updateWithFocused(focused, date.year());
        break;
    case 3:
        actives = updateWithFocused(focused);
        break;
    default:
        break;
    }
    table_widget_->update(actives);
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
} // namespace apptime