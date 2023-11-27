#include "ignore.hpp"

#include <QComboBox>
#include <QLineEdit>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFormLayout>

namespace apptime {
ignore_window::ignore_window(QWidget *parent) : QWidget{parent}, table_widget_{new QTableWidget{this}} {
    setWindowFlag(Qt::Window);
    setWindowTitle("Ignore list");

    // table
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);
    addTable();

    // buttons
    QHBoxLayout *hbox = new QHBoxLayout;
    QPushButton *add  = new QPushButton{"Add"};
    hbox->addWidget(add);
    QPushButton *remove = new QPushButton{"Remove"};
    hbox->addWidget(remove);
    vbox->addLayout(hbox);

    // connect
    connect(add, &QAbstractButton::clicked, this, &ignore_window::openAddDialog);
    connect(remove, &QAbstractButton::clicked, this, &ignore_window::removeSelected);
}

void ignore_window::addTable() {
    const auto main_layout = static_cast<QVBoxLayout *>(layout());
    main_layout->addWidget(table_widget_);

    table_widget_->verticalHeader()->hide();
    table_widget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_widget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_widget_->setContextMenuPolicy(Qt::CustomContextMenu);

    table_widget_->setColumnCount(2);
    QStringList header;
    header << "Type"
           << "Path";
    table_widget_->setHorizontalHeaderLabels(header);
}

void ignore_window::update(const std::vector<ignore> &list) {
    list_ = list;
    table_widget_->clearContents();

    table_widget_->setRowCount(static_cast<int>(list.size()));
    for (int i = 0; i < list.size(); i++) {
        const auto &v = list[i];

        QTableWidgetItem *type = new QTableWidgetItem{QString::fromStdString(v.first == ignore_file ? "File" : "Path")};
        type->setFlags(type->flags() & ~Qt::ItemIsEditable);
        table_widget_->setItem(i, 0, type);

        QTableWidgetItem *path = new QTableWidgetItem{QString::fromStdString(v.second)};
        path->setFlags(path->flags() & ~Qt::ItemIsEditable);
        table_widget_->setItem(i, 1, path);
    }

    table_widget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_widget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

void ignore_window::openAddDialog() {
    if (!dialog_add_) {
        dialog_add_         = new QDialog{this};
        QFormLayout *layout = new QFormLayout{dialog_add_};

        QComboBox *type_combo = new QComboBox{dialog_add_};
        type_combo->addItem("Path");
        type_combo->addItem("File");
        QLineEdit *path_edit = new QLineEdit{dialog_add_};

        layout->addRow("Select a type:", type_combo);
        layout->addRow("Input a path:", path_edit);

        QPushButton *ok = new QPushButton{"OK", dialog_add_};

        layout->addWidget(ok);
        dialog_add_->setLayout(layout);

        connect(ok, &QPushButton::clicked, [this, type_combo, path_edit]() {
            emit needToAdd(static_cast<ignore_type>(type_combo->currentIndex()), path_edit->text().toStdString());
            type_combo->setCurrentIndex(0);
            path_edit->setText("");
            dialog_add_->close();
        });
    }

    dialog_add_->open();
    dialog_add_->setFixedSize(dialog_add_->size());
}

void ignore_window::removeSelected() {
    const QItemSelectionModel *select = table_widget_->selectionModel();
    if (!select->hasSelection()) {
        return;
    }
    const QModelIndexList rows = select->selectedRows();
    if (rows.size() < 1 || rows.size() > list_.size()) {
        return;
    }

    const int row = rows.first().row();
    emit      needToRemove(list_[row].first, list_[row].second);
}
} // namespace apptime