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
// the SPC convective outlook, Colorado State's machine-learning probabilities (CSU-MLP) and the CIPS analog
// guidance from Saint Louis University. CIPS has a picture per hazard for days 1-2, one all-hazards picture for
// days 3-6 and nothing after that; CSU-MLP's day 1-2 pictures already show all three hazards.
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
    void resizeEventCustom() override;

    VBox box;
    HBox rowTop;
    HBox rowTitles;
    HBox rowImages;
    ComboBox comboDay;
    ComboBox comboHazard;
    Text textNote;
    // deques: elements must not move once their views are in the layout
    std::deque<Text> titles;
    std::deque<Image> images;
    int generation{0};
};

#endif  // SVRCOMPARISON_H
