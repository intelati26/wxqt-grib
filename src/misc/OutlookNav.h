// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OUTLOOKNAV_H
#define OUTLOOKNAV_H

#include <functional>
#include <string>
#include <vector>
#include "ui/ComboBox.h"
#include "ui/Window.h"

// A "go to" list for the forecast product screens of the SPC, WPC and NHC, so each is a stop on the way to the others instead of a dead end: the convective summary and days, the fire
// weather summary and days, the thunderstorm outlooks, the state graphics, storm reports, mesoanalysis, the excessive rainfall outlooks, the national text and images and the
// tropical screens. Choosing one opens it in a window of its own and leaves this one where it is. Put combo() in the screen's layout.
class OutlookNav {
public:
    enum Kind { SevereSummary, SevereDay, FireSummary, FireDay, Thunderstorm, State, StormReports, Mesoanalysis, Comparison, CompositeMap, Dashboard, RainfallSummary, RainfallDay, NationalText, NationalImages, NhcProducts, Hurricanes, TropicalHub };
    // owner: the screen the list is in; kind and value: what that screen is (the day for SevereDay / State, 0 .. 2 for FireDay, 0 today / 1 yesterday for StormReports, 0 .. 4 for RainfallDay); it is left out of the list
    OutlookNav(Window * owner, Kind kind, int value = 0);
    ComboBox& combo() { return comboBox; }

private:
    void choose();
    Window * owner;
    ComboBox comboBox;
    struct Item {
        Kind kind;
        int value;
    };
    std::vector<Item> items;   // what each entry of the list opens, entry 0 being the prompt
};

#endif  // OUTLOOKNAV_H
