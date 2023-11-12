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
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel{"Select a date:"});
    date_widget_ = new QDateEdit{QDate::currentDate()};
    hbox->addWidget(date_widget_);

    static_cast<QVBoxLayout *>(centralWidget()->layout())->addLayout(hbox);
}

void window::addSeparator() {
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);

    centralWidget()->layout()->addWidget(line);
}

void window::addResults() {
    table_widget_ = new table;
    table_widget_->update(db_.actives());
    centralWidget()->layout()->addWidget(table_widget_);
}
} // namespace apptime