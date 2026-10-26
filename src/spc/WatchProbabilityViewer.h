// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WATCHPROBABILITYVIEWER_H
#define WATCHPROBABILITYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QLabel>
#include <QString>
#include <QTableWidget>
#include "spc/UtilityWatchProbabilities.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The table of the SPC's watch hazard probabilities, a row for each watch and a column for each hazard (tornadoes, strong tornadoes, severe wind, wind over 65 kt, severe hail, hail over 2 in, combined),
// to read, and to export (the shared table export: which watches, which hazards, per cent and / or level, the layout, a first line; copy for a spreadsheet, CSV, a picture) as the forecast points are.
class WatchProbabilityViewer : public Window {
public:
    struct Watch {
        int number{0};
        QString title;     // "Tornado Watch 693"
        long end{0};       // seconds since 1970, 0 when not known
    };
    WatchProbabilityViewer(Window * parent, const std::vector<Watch>& watches);

private:
    struct Row {
        Watch watch;
        std::vector<UtilityWatchProbabilities::Hazard> hazards;
        bool read{false};
    };
    void load();
    void fill();
    void exportTable();
    void closeEventCustom() override { closed = true; }
    VBox box;
    QTableWidget * table{};
    QLabel * status{};
    std::vector<Watch> watches;
    std::shared_ptr<std::vector<Row>> rows;
    int generation{0};
    bool closed{false};
};

#endif  // WATCHPROBABILITYVIEWER_H
