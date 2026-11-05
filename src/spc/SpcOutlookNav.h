// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPCOUTLOOKNAV_H
#define SPCOUTLOOKNAV_H

#include <functional>
#include <string>
#include <vector>
#include "ui/ComboBox.h"
#include "ui/Window.h"

// A "go to" list for the SPC's outlook screens, so each is a stop on the way to the others instead of a dead end: the convective summary and days 1-3 and 4-8, the fire weather
// summary and days, the thunderstorm outlooks and the state graphics. Choosing one opens it in place of the screen it was chosen in. Put combo() in the screen's layout.
class SpcOutlookNav {
public:
    enum Kind { SevereSummary, SevereDay, FireSummary, FireDay, Thunderstorm, State };
    // owner: the screen the list is in; kind and value: what that screen is (the day for SevereDay / State, 0 .. 2 for FireDay); it is left out of the list
    SpcOutlookNav(Window * owner, Kind kind, int value = 0);
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

#endif  // SPCOUTLOOKNAV_H
