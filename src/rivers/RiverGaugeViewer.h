// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RIVERGAUGEVIEWER_H
#define RIVERGAUGEVIEWER_H

#include <deque>
#include <memory>
#include <string>
#include "rivers/HydrographChart.h"
#include "rivers/UtilityRivers.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/FlowBox.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// One river gauge: the hydrograph (observed, the NWS forecast when there is one, and the National Water Model) with the flood-stage lines,
// and the numbers around it - now, modelled, and the record (crests, outlook, what each stage floods).
class RiverGaugeViewer : public Window {
public:
    RiverGaugeViewer(Window * parent, const string& lid);

private:
    void load();
    void build();
    void drawChart();
    void closeEventCustom() override { closed = true; }
    string currentText() const;
    string modelText() const;
    string historyText() const;
    string impactsText() const;

    string lid;
    std::shared_ptr<UtilityRivers::Detail> detail;
    VBox box;
    ScrolledWindow sw;
    HBox rowTop;
    ComboBox comboMode;
    ComboBox comboRange;
    Button buttonRefresh;
    Button buttonImpacts;
    Text textTitle;
    Text textSubtitle;
    Text textNow;
    HydrographChart * chart;
    Text headingCurrent;
    Text textCurrent;
    Text headingModel;
    Text textModel;
    Text headingHistory;
    Text textHistory;
    FlowBox flowImages;
    std::deque<Image> images;
    bool closed{false};
};

#endif  // RIVERGAUGEVIEWER_H
