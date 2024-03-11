#include "records.hpp"

#include <format>
#include <set>

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMenu>

#include "platforms/icon.hpp"

using apptime::table_records;

std::string application_name(const apptime::record &app, bool window_names = false) {
    if (window_names && !app.name.empty()) {
        return app.name;
    }
    const std::filesystem::path path{app.path};
    return path.filename().string();
}

table_records::app_duration_t total_duration(const apptime::record &app) {
    using namespace std::chrono;

    system_clock::duration result{};
    for (const auto &[start, end]: app.times) {
        result += end - start;
    }
    return hh_mm_ss{result};
}

auto convert_actives(const std::vector<apptime::record> &apps, bool window_names = false) {
    // std::set is needed here for sorting applications
    const auto comp = [](const table_records::table_info &left, const table_records::table_info &right) {
        return left.duration.to_duration() > right.duration.to_duration();
    };
    std::set<table_records::table_info, decltype(comp)> result{comp};

    for (const auto &app: apps) {
        result.emplace(app.path, application_name(app, window_names), total_duration(app));
    }
    return std::vector<table_records::table_info>{result.begin(), result.end()};
}

// format the duration to 999h 59m 59s
std::string duration_format(const table_records::app_duration_t &duration) {
    std::string result;

    const auto hours   = duration.hours().count();
    const auto minutes = duration.minutes().count();
    const auto seconds = duration.seconds().count();
    if (hours) {
        std::format_to(std::back_inserter(result), "{:d}h ", hours);
    }
    if (minutes) {
        std::format_to(std::back_inserter(result), "{:d}m ", minutes);
    }
    std::format_to(std::back_inserter(result), "{:d}s", seconds);
    return result;
}

namespace apptime {
table_records::table_records(QWidget *parent) : QTableWidget{parent}, contextMenu_{new QMenu{"Context menu", this}} {
    setShowGrid(false);
    verticalHeader()->hide();
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    loadStyle();

    setColumnCount(2);
    setHorizontalHeaderLabels({"Using time", " "});
    horizontalHeaderItem(0)->setTextAlignment(Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignVCenter);

    connect(this, &QTableWidget::customContextMenuRequested, this, &table_records::showContextMenu);
}

void table_records::update(const std::vector<record> &apps, const settings &set) {
    clearContents();

    list_ = convert_actives(apps, set.window_names);
    setRowCount(static_cast<int>(list_.size()));
    for (int i = 0; i < static_cast<int>(list_.size()); i++) {
        const auto &element = list_[i];

        auto *name = new QTableWidgetItem{QString::fromStdString(element.name)};
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        setItem(i, 0, name);
        if (set.icons) {
            name->setIcon(application_icon(element.path));
        }

        const std::string total_str = duration_format(element.duration);
        auto             *duration  = new QTableWidgetItem{QString::fromStdString(total_str)};
        duration->setFlags(duration->flags() & ~Qt::ItemIsEditable);
        duration->setTextAlignment(Qt::AlignmentFlag::AlignRight | Qt::AlignmentFlag::AlignVCenter);
        setItem(i, 1, duration);
    }

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void table_records::showContextMenu(const QPoint &pos) {
    contextMenu_->clear();
    const QTableWidgetItem *item = itemAt(pos);
    if (!item) {
        return;
    }

    auto *copy_path = new QAction{"Copy full path", this};
    auto *copy_name = new QAction{"Copy name", this};
    contextMenu_->addAction(copy_path);
    contextMenu_->addAction(copy_name);
    contextMenu_->addSeparator();

    auto *ignore = new QAction{"Ignore the application"};
    contextMenu_->addAction(ignore);

    connect(copy_path, &QAction::triggered, [this, item]() {
        const std::string &text = list_[item->row()].path;
        QGuiApplication::clipboard()->setText(QString::fromStdString(text));
    });
    connect(copy_name, &QAction::triggered, [this, item]() {
        const std::string &text = list_[item->row()].name;
        QGuiApplication::clipboard()->setText(QString::fromStdString(text));
    });
    connect(ignore, &QAction::triggered, [this, item]() {
        const std::string &path = list_[item->row()].path;
        emit               addIgnore(ignore_file, path);
    });

    contextMenu_->exec(mapToGlobal(pos));
}

void table_records::loadStyle() {
    QFile style_file{":/styles/records.qss"};
    style_file.open(QFile::ReadOnly);

    const QString style{style_file.readAll()};
    setStyleSheet(style);
}
} // namespace apptime