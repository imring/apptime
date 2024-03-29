#ifndef APPTIME_GUI_IGNORE_HPP
#define APPTIME_GUI_IGNORE_HPP

#include <QDialog>
#include <QTableWidget>

#include "database/database.hpp"

namespace apptime {
class ignore_window : public QWidget {
    Q_OBJECT

public:
    explicit ignore_window(QWidget *parent = nullptr);

    void update(const std::vector<ignore> &list);

signals:
    void needToAdd(ignore_type type, std::string_view path);
    void needToRemove(ignore_type type, std::string_view path);

public slots:
    void openAddDialog();
    void removeSelected();

private:
    void addTable();
    void createAddDialog();

    QTableWidget *table_widget_ = nullptr;
    QDialog      *dialog_add_   = nullptr;

    std::vector<ignore> list_;
};
} // namespace apptime

#endif // APPTIME_GUI_IGNORE_HPP