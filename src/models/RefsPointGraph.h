// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef REFSPOINTGRAPH_H
#define REFSPOINTGRAPH_H

#include <string>
#include <vector>
#include <QWidget>
#include "models/UtilityRefs.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// Stage 5 of docs/refs-viewer-plan.md: the "plume" window. One line per RRFS
// Ensemble member of a field's value at a clicked point across forecast
// hours, plus a bold ensemble-mean line (and a dashed threshold line for
// Paintball / Member Probability fields). Hours are fetched one at a time,
// off the UI thread, and the chart fills in as each hour lands - cancelled if
// the window is closed or the horizon is changed.
class PlumeCanvas;

class RefsPointGraph : public Window {
public:
    RefsPointGraph(Window * parent, const UtilityRefs::MemberBasis& basis, double lon, double lat,
                   const string& runId);

private:
    void start();
    void fetchHour(size_t hourIndex, int generation);
    void closeEventCustom() override;

    UtilityRefs::MemberBasis basis;
    double lon;
    double lat;
    string runId;
    string dateStr;
    string cycle;

    VBox box;
    HBox rowTop;
    Text textInfo;
    ComboBox comboHorizon;
    PlumeCanvas * canvas;

    int generation{0};
    vector<int> hours;
    vector<vector<double>> pendingValues;   // one entry per member, filled by the worker for the current hour
    string pendingStatus;
};

#endif  // REFSPOINTGRAPH_H
