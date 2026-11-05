// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "SpcOutlookNav.h"
#include <algorithm>
#include <QTimer>
#include "spc/SpcFireSummary.h"
#include "spc/SpcFireWeatherOutlook.h"
#include "spc/SpcSwoDay1.h"
#include "spc/SpcSwoStateGraphics.h"
#include "spc/SpcSwoSummary.h"
#include "spc/SpcTstormOutlooks.h"

SpcOutlookNav::SpcOutlookNav(Window * owner, Kind kind, int value)
    : owner{owner}
    , comboBox{owner}
{
    vector<string> labels{"Go to another SPC outlook..."};
    auto add = [&] (Kind k, int v, const string& label) {
        // the screen itself is left out; a fire weather screen has all of its own days in a list already
        if ((k == kind && v == value) || (k == FireDay && kind == FireDay)) {
            return;
        }
        items.push_back({k, v});
        labels.push_back(label);
    };
    items.clear();
    items.push_back({SevereSummary, 0});   // the prompt: nothing opens
    add(SevereSummary, 0, "Convective outlooks, days 1-8 summary");
    add(SevereDay, 1, "Convective outlook, day 1");
    add(SevereDay, 2, "Convective outlook, day 2");
    add(SevereDay, 3, "Convective outlook, day 3");
    add(SevereDay, 4, "Convective outlook, days 4-8");
    add(FireSummary, 0, "Fire weather outlooks, summary");
    add(FireDay, 0, "Fire weather outlook, day 1");
    add(FireDay, 1, "Fire weather outlook, day 2");
    add(FireDay, 2, "Fire weather outlook, days 3-8");
    add(Thunderstorm, 0, "Thunderstorm outlooks");
    if (kind == SevereDay || kind == State) {
        const int day = std::min(value, 3);
        add(State, day, "State graphics, day " + std::to_string(day));
    }
    comboBox.setList(labels);
    comboBox.setIndex(0);
    comboBox.connect([this] { choose(); });
}

void SpcOutlookNav::choose() {
    const auto index = static_cast<size_t>(comboBox.getIndex());
    if (index == 0 || index >= items.size()) {
        return;
    }
    const auto item = items[index];
    auto * window = owner;
    auto * parent = dynamic_cast<Window *>(window->parentWidget());
    if (parent == nullptr) {
        parent = window;
    }
    switch (item.kind) {
        case SevereSummary: new SpcSwoSummary{parent}; break;
        case SevereDay: new SpcSwoDay1{parent, item.value}; break;
        case FireSummary: new SpcFireSummary{parent}; break;
        case FireDay: new SpcFireWeatherOutlook{parent, item.value}; break;
        case Thunderstorm: new SpcTstormOutlooks{parent}; break;
        case State: new SpcSwoStateGraphics{parent, item.value}; break;
    }
    QTimer::singleShot(0, window, [window] { window->close(); });   // the new screen replaces this one
}
