#ifndef APPTIME_GUI_TABLE_HPP
#define APPTIME_GUI_TABLE_HPP

#include <QTableWidget>

#include "../database.hpp"

namespace apptime {
class table : public QTableWidget {
public:
    table(QWidget *parent = nullptr);

    void update(const std::vector<active> &apps);
};
} // namespace apptime

#endif // APPTIME_GUI_TABLE_HPP