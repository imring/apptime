#ifndef APPTIME_GUI_TABLE_HPP
#define APPTIME_GUI_TABLE_HPP

#include <chrono>

#include <QTableWidget>

#include "../../database.hpp"

namespace apptime {
class table_records : public QTableWidget {
    Q_OBJECT

public:
    using app_duration_t = std::chrono::hh_mm_ss<std::chrono::system_clock::duration>;
    struct table_info {
        std::string    path;
        std::string    name;
        app_duration_t duration;
    };

    struct settings {
        // show the window name instead of the file name
        bool window_names;
        // show application icons
        bool icons;
    };

    table_records(QWidget *parent = nullptr);

    void update(const std::vector<record> &apps, const settings &set = {});

signals:
    void addIgnore(ignore_type type, std::string_view path);

private slots:
    void showContextMenu(const QPoint &pos);

private:
    void loadStyle();

    QMenu                  *contextMenu_ = nullptr;
    std::vector<table_info> list_;
};
} // namespace apptime

#endif // APPTIME_GUI_TABLE_HPP