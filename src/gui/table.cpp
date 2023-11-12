#include "table.hpp"

#include <format>
#include <chrono>
#include <algorithm>

#include <QHeaderView>

using namespace std::chrono;
using app_duration_t = hh_mm_ss<system_clock::duration>;

std::string application_name(const apptime::active &app) {
    if (!app.name.empty()) {
        return app.name;
    }
    const std::filesystem::path path{app.path};
    return path.filename().string();
}

hh_mm_ss<system_clock::duration> total_duration(const apptime::active &app) {
    system_clock::duration result{};
    for (const auto &[start, end]: app.times) {
        result += end - start;
    }
    return hh_mm_ss{result};
}

auto convert_actives(const std::vector<apptime::active> &apps) {
    using result_t = std::pair<std::string, app_duration_t>;
    std::vector<result_t> result;

    result.reserve(apps.size());
    std::ranges::transform(apps, std::back_inserter(result), [](const apptime::active &app) {
        return std::make_pair(application_name(app), total_duration(app));
    });
    std::ranges::sort(result, [](const result_t &a, const result_t &b) {
        return a.second.to_duration() > b.second.to_duration();
    });
    return result;
}

namespace apptime {
table::table(QWidget *parent) : QTableWidget{parent} {
    verticalHeader()->hide();
    setSelectionMode(QAbstractItemView::NoSelection);

    setColumnCount(2);
    QStringList header;
    header << "Name"
           << "Time";
    setHorizontalHeaderLabels(header);
}

void table::update(const std::vector<active> &apps) {
    clearContents();

    const auto converted_apps = convert_actives(apps);
    setRowCount(converted_apps.size());
    for (int i = 0; i < converted_apps.size(); i++) {
        const auto &v = converted_apps[i];

        QTableWidgetItem *name = new QTableWidgetItem{QString::fromStdString(v.first)};
        name->setFlags(name->flags() & ~Qt::ItemIsEditable);
        setItem(i, 0, name);

        const std::string total_str = std::format("{:02d}:{:02d}:{:02d}", v.second.hours().count(), v.second.minutes().count(), v.second.seconds().count());
        QTableWidgetItem *duration  = new QTableWidgetItem{QString::fromStdString(total_str)};
        duration->setFlags(duration->flags() & ~Qt::ItemIsEditable);
        setItem(i, 1, duration);
    }

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}
} // namespace apptime