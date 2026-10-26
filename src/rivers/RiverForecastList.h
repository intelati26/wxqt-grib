// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RIVERFORECASTLIST_H
#define RIVERFORECASTLIST_H

#include <map>
#include <memory>
#include <set>
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
// when it is first at action stage and at minor flooding (the forecasts of the 24 hour steps), how the crest compares with the gauge's record crests (read after the table is up, for the gauges at
// action stage or higher), and the thresholds. The category to start from, "only those not there now" and "pin the crest to now" narrow or change it; "low water" lists the gauges forecast to fall
// below their low-water level instead. It sorts by any column, a double-click opens the gauge's page, and Export... saves it as CSV or a picture or copies it for a spreadsheet.
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
        string trend;           // for the Note column
        bool onlyNow{false};    // not in the forecast layer: a gauge at action stage or higher now whose forecast does not stay there
        string firstAction, firstMinor;   // when the forecast first has it at that category or above ("Tue 27 Oct 12:00Z (+48 h)")
    };
    void load();
    void fill();
    void startRecords();                       // the record crests of the gauges listed, a few at a time, after the table is up
    void showRecords();                        // the "crest vs record" cells from what has been read so far
    QString recordText(const Row&) const;
    void exportTable();
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * minimum{};
    QCheckBox * onlyRising{};
    QCheckBox * pin{};
    QLabel * status{};
    QTableWidget * table{};
    std::shared_ptr<vector<Row>> rows;
    std::shared_ptr<vector<Row>> lows;         // the gauges forecast to fall below the low-water level
    vector<Row> shown;
    std::map<string, vector<std::pair<double, string>>> records;   // gauge id -> its record crests (stage, time), highest first
    std::set<string> asked;
    size_t recordsDone{0}, recordsWanted{0};
    int generation{0};
    bool closed{false};
};

#endif  // RIVERFORECASTLIST_H
