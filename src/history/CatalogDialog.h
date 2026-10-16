// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CATALOGDIALOG_H
#define CATALOGDIALOG_H

#include <QDialog>
#include <functional>
#include <memory>
#include <vector>
#include "history/HistoryEvent.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class Window;

// The catalogs of the History page: lists of things that happened (tornado outbreaks, the biggest days at a place, tropical storms ...) to take an event from. A catalog is a Source:
// its columns, its ways of ordering, what it is loaded from (a function, run off the interface thread) and the event each of its entries makes. The dialog is the same for all of
// them; a new kind of catalog is one more Source (history/CatalogSources).
class CatalogDialog : public QDialog {
public:
    struct Entry {
        QStringList cells;                 // the row of the table
        std::vector<double> keys;          // one per way of ordering (the bigger first)
        double size{0.0};                  // what the "at least" box compares with (tornadoes, inches ...)
        int year{0};
        HistoryEvent event;                // the event it makes (id and catalog id are set by the page)
    };
    struct Params {
        int fromYear{1950};
        int toYear{2100};
        int what{0};                       // the Source's own choice (the measure of the biggest days)
        double lat{0.0};
        double lon{0.0};
    };
    struct Source {
        QString name;
        QStringList columns;
        QStringList orders;
        QStringList whats;                 // empty: no such choice
        QString sizeLabel;                 // "tornadoes": the "at least" box, empty: none
        int sizeDefault{0};
        bool place{false};                 // made for the page's point: the load is made again when the place or the choice changes (a button asks for it)
        QString note;
        std::function<std::vector<Entry>(const Params&, std::string& error)> load;
    };
    CatalogDialog(Window * owner, std::vector<Source> sources, double lat, double lon, std::function<void(const HistoryEvent&)> add);

private:
    void reload();                         // read the chosen catalog (off the interface thread)
    void fill();                           // the table, from what is loaded and the filters
    Window * owner;
    std::vector<Source> sources;
    std::function<void(const HistoryEvent&)> add;
    double lat, lon;
    int generation{0};
    std::vector<Entry> loaded;
    std::vector<const Entry *> shown;
    std::string error;
    QComboBox * kind{};
    QComboBox * what{};
    QComboBox * order{};
    QSpinBox * from{};
    QSpinBox * to{};
    QSpinBox * size{};
    QLabel * sizeText{};
    QLabel * whatText{};
    QLabel * status{};
    QPushButton * load{};
    QPushButton * addButton{};
    QTableWidget * table{};
};

#endif  // CATALOGDIALOG_H
