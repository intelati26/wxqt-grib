// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SVRCOMPARISON_H
#define SVRCOMPARISON_H

#include <deque>
#include <string>
#include <vector>
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// Severe-weather outlooks side by side, by day (the idea of the NWS St. Louis "CIPS/CSU/SPC Comparison" page):
// the SPC convective outlook, Colorado State's machine-learning probabilities (CSU-MLP) and the ABPG analog
// guidance of the University of Missouri (which replaces SLU's CIPS, no longer updated). ABPG publishes regional
// maps (8 regions) of the percentage of its top analogs with 1+ or 5+ severe reports, out to 6 days; its
// pictures state their own valid time. CSU-MLP's day 1-2 pictures already show all three hazards.
class SvrComparison : public Window {
public:
    explicit SvrComparison(Window * parent);

private:
    struct Panel {
        string title;
        string url;       // "" = nothing for this day
    };
    vector<Panel> panels(int day, int hazard) const;
    void reload();
    void setValid(size_t panel, const string& text);
    void resizeEventCustom() override;

    VBox box;
    HBox rowTop;
    HBox rowTitles;
    HBox rowValid;
    HBox rowImages;
    ComboBox comboDay;
    ComboBox comboHazard;   // ABPG: 1+ or 5+ severe reports
    ComboBox comboRegion;   // ABPG region
    Text textNote;
    // deques: elements must not move once their views are in the layout
    std::deque<Text> titles;
    std::deque<Text> valids;   // under each title: the period the picture is valid for
    std::deque<Image> images;
    int generation{0};
};

#endif  // SVRCOMPARISON_H
