// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RIVERFORECASTLIST_H
#define RIVERFORECASTLIST_H

#include <memory>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include "rivers/UtilityRivers.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The river gauges forecast to reach the action stage or higher (the NWS river forecasts' highest category over the whole forecast), as a table: where, what it is now, the forecast crest and when,
// the thresholds. The category to start from and "only those not there now" (the ones forecast to rise into it) narrow it; it sorts by any column, a click opens the gauge's hydrograph, and
// Export... saves it as CSV or a picture (the choices of which gauges and columns) or copies it for a spreadsheet.
class RiverForecastList : public Window {
public:
    explicit RiverForecastList(Window * parent);
    void exportNow() { exportTable(); }   // the Export... button's work (a development route calls it)

private:
    struct Row {
        UtilityRivers::Peak peak;
        string nowStatus;       // the gauge's category now
        double observed{UtilityRivers::none};
        string obsTime;
    };
    void load();
    void fill();
    void exportTable();
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * minimum{};
    QCheckBox * onlyRising{};
    QLabel * status{};
    QTableWidget * table{};
    std::shared_ptr<vector<Row>> rows;
    vector<Row> shown;
    int generation{0};
    bool closed{false};
};

#endif  // RIVERFORECASTLIST_H
