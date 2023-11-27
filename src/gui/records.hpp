#ifndef APPTIME_GUI_TABLE_HPP
#define APPTIME_GUI_TABLE_HPP

#include <chrono>

#include <QTableWidget>

#include "../database.hpp"

namespace apptime {
class records : public QTableWidget {
    Q_OBJECT

public:
    using app_duration_t = std::chrono::hh_mm_ss<std::chrono::system_clock::duration>;
    struct table_info {
        std::string    path;
        std::string    name;
        app_duration_t duration;
    };

    records(QWidget *parent = nullptr);

    void update(const std::vector<record> &apps);

signals:
    void addIgnore(ignore_type type, std::string_view path);

private:
    void showContextMenu(const QPoint &pos);

    QMenu                  *contextMenu_;
    std::vector<table_info> list_;
};
} // namespace apptime

#endif // APPTIME_GUI_TABLE_HPP