#ifndef APPTIME_GUI_WINDOW_HPP
#define APPTIME_GUI_WINDOW_HPP

#include <QDateEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QMainWindow>

#include "../monitoring.hpp"
#include "records.hpp"
#include "ignore.hpp"

// TODO:
// - app histogram;
// - style?

namespace apptime {
class window : public QMainWindow {
    Q_OBJECT
public:
    window(QWidget *parent = nullptr);

    void closeEvent(QCloseEvent *event) override;

    void toggle();
    bool running() const {
        return monitor_.running();
    }

public slots:
    void updateFormat(int index);
    void openIgnoreWindow();
    void addIgnore(ignore_type type, std::string_view path);
    void removeIgnore(ignore_type type, std::string_view path);

private:
    enum class DateFormat { Day, Month, Year, All };
    static QString getDateFormat(DateFormat format);
    static DateFormat getDateFormat(QAnyStringView format) ;

    void addMenubar();
    void addOptionalWidgets();
    void addSeparator();
    void addResults();

    void readSettings();
    void saveSettings();

    void updateRecords();

    QCheckBox *focus_widget_  = nullptr;
    QComboBox *filter_widget_ = nullptr;
    QDateEdit *date_widget_   = nullptr;
    records   *table_widget_  = nullptr;

    QAction *toggle_names_ = nullptr;
    QAction *toggle_icons_ = nullptr;

    ignore_window *ignore_window_ = nullptr;

    database   db_;
    monitoring monitor_;
};
} // namespace apptime

#endif // APPTIME_GUI_WINDOW_HPP