// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "OutlookNav.h"
#include <algorithm>
#include "dashboard/Dashboards.h"
#include "hurricane/HurricaneViewer.h"
#include "nhc/Nhc.h"
#include "spc/SpcFireSummary.h"
#include "spc/SpcFireWeatherOutlook.h"
#include "misc/SevereDashboard.h"
#include "spc/SpcCompMap.h"
#include "spc/SpcMeso.h"
#include "spc/SpcStormReports.h"
#include "spc/SvrComparison.h"
#include "spc/SpcSwoDay1.h"
#include "spc/SpcSwoStateGraphics.h"
#include "spc/SpcSwoSummary.h"
#include "spc/SpcTstormOutlooks.h"
#include "wpc/NationalImages.h"
#include "wpc/NationalText.h"
#include "wpc/RainfallOutlook.h"
#include "wpc/RainfallOutlookSummary.h"

OutlookNav::OutlookNav(Window * owner, Kind kind, int value)
    : owner{owner}
    , comboBox{owner}
{
    vector<string> labels{"Go to another outlook..."};
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
    add(SevereSummary, 0, "SPC: Convective outlooks, days 1-8 summary");
    add(SevereDay, 1, "SPC: Convective outlook, day 1");
    add(SevereDay, 2, "SPC: Convective outlook, day 2");
    add(SevereDay, 3, "SPC: Convective outlook, day 3");
    add(SevereDay, 4, "SPC: Convective outlook, days 4-8");
    add(FireSummary, 0, "SPC: Fire weather outlooks, summary");
    add(FireDay, 0, "SPC: Fire weather outlook, day 1");
    add(FireDay, 1, "SPC: Fire weather outlook, day 2");
    add(FireDay, 2, "SPC: Fire weather outlook, days 3-8");
    add(Thunderstorm, 0, "SPC: Thunderstorm outlooks");
    add(StormReports, 0, "SPC: Storm reports, today");
    add(StormReports, 1, "SPC: Storm reports, yesterday");
    add(Mesoanalysis, 0, "SPC: Mesoanalysis");
    add(Comparison, 0, "SPC: Severe outlook comparison (SPC, CSU-MLP, CIPS)");
    add(CompositeMap, 0, "SPC: Composite map");
    add(Dashboard, 0, "SPC: Severe dashboard");
    add(RainfallSummary, 0, "WPC: Excessive rainfall outlooks, summary");
    for (int day = 0; day < 5; day++) {
        add(RainfallDay, day, "WPC: Excessive rainfall outlook, day " + std::to_string(day + 1));
    }
    add(NationalText, 0, "WPC: National forecast text");
    add(NationalImages, 0, "WPC: National forecast images");
    add(NhcProducts, 0, "NHC: Product viewer");
    add(Hurricanes, 0, "NHC: Tropical cyclones (Atlantic, East and Central Pacific)");
    add(TropicalHub, 0, "NHC: Tropical Hub");
    if (kind == SevereDay || kind == State) {
        const int day = std::min(value, 3);
        add(State, day, "SPC: State graphics, day " + std::to_string(day));
    }
    comboBox.setList(labels);
    comboBox.setIndex(0);
    comboBox.connect([this] { choose(); });
}

void OutlookNav::choose() {
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
        case StormReports: new SpcStormReports{parent, item.value == 0 ? "today" : "yesterday"}; break;
        case Mesoanalysis: new SpcMeso{parent}; break;
        case Comparison: new SvrComparison{parent}; break;
        case CompositeMap: new SpcCompMap{parent}; break;
        case Dashboard: new SevereDashboard{parent}; break;
        case RainfallSummary: new RainfallOutlookSummary{parent}; break;
        case RainfallDay: new RainfallOutlook{parent, item.value}; break;
        case NationalText: new ::NationalText{parent}; break;
        case NationalImages: new ::NationalImages{parent}; break;
        case NhcProducts: new Nhc{parent}; break;
        case Hurricanes: new HurricaneViewer{parent}; break;
        case TropicalHub: Dashboards::openTropicalHub(parent); break;
    }
    comboBox.block();   // the list shows its prompt again: this screen stays, the choice opened beside it
    comboBox.setIndex(0);
    comboBox.unblock();
}
