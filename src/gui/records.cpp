#include "records.hpp"

#include <algorithm>
#include <format>

#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMenu>

#include "platforms/icon.hpp"

std::string application_name(const apptime::record &app, bool window_names = false) {
    if (window_names && !app.name.empty()) {
        return app.name;
    }
    const std::filesystem::path path{app.path};
    return path.filename().string();
}

apptime::records::app_duration_t total_duration(const apptime::record &app) {
    using namespace std::chrono;

    system_clock::duration result{};
    for (const auto &[start, end]: app.times) {
        result += end - start;
    }
    return hh_mm_ss{result};
}

auto convert_actives(const std::vector<apptime::record> &apps, bool window_names = false) {
    std::vector<apptime::records::table_info> result;

    result.reserve(apps.size());
    std::ranges::transform(apps, std::back_inserter(result), [window_names](const apptime::record &app) {
        return apptime::records::table_info{app.path, application_name(app, window_names), total_duration(app)};
    });
    std::ranges::sort(result, [](const apptime::records::table_info &left, const apptime::records::table_info &right) {
        return left.duration.to_duration() > right.duration.to_duration();
    });
    return result;
}

namespace apptime {
records::records(QWidget *parent) : QTableWidget{parent}, contextMenu_{new QMenu{"Context menu", this}} {
    verticalHeader()->hide();
    setSelectionMode(QAbstractItemView::NoSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);

    setColumnCount(2);
    QStringList header;
    header << "Name"
           << "Time";
    setHorizontalHeaderLabels(header);

    connect(this, &QTableWidget::customContextMenuRequested, this, &records::showContextMenu);
}

void records::update(const std::vector<record> &apps, const settings &set) {
    clearContents();

    list_ = convert_actives(apps, set.window_names);
    setRowCount(static_cast<int>(list_.size()));
    for (int i = 0; i < list_.size(); i++) {
        const auto &element = list_[i];

        auto *name = new QTableWidgetItem{QString::fromStdString(element.name)};
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        setItem(i, 0, name);
        if (set.icons) {
            name->setIcon(application_icon(element.path));
        }

        const std::string total_str =
            std::format("{:02d}:{:02d}:{:02d}", element.duration.hours().count(), element.duration.minutes().count(), element.duration.seconds().count());
        auto *duration = new QTableWidgetItem{QString::fromStdString(total_str)};
        duration->setFlags(duration->flags() & ~Qt::ItemIsEditable);
        setItem(i, 1, duration);
    }

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void records::showContextMenu(const QPoint &pos) {
    contextMenu_->clear();
    QTableWidgetItem *item = itemAt(pos);
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

    const QAction *action = contextMenu_->exec(mapToGlobal(pos));
    if (!action) {
        return;
    }

    if (action == copy_path) {
        const std::string &text = list_[item->row()].path;
        QGuiApplication::clipboard()->setText(QString::fromStdString(text));
    } else if (action == copy_name) {
        const std::string &text = list_[item->row()].name;
        QGuiApplication::clipboard()->setText(QString::fromStdString(text));
    } else if (action == ignore) {
        const std::string &path = list_[item->row()].path;
        emit               addIgnore(ignore_file, path);
    }
}
} // namespace apptime