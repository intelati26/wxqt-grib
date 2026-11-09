// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "OutlookNav.h"
#include "misc/OutlookScreen.h"
#include "misc/NiceWeatherOutlook.h"
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

const std::vector<OutlookNav::Destination>& OutlookNav::destinations() {
    static const std::vector<Destination> all{
        {SevereSummary, 0, "SPC: Convective outlooks, days 1-8 summary"},
        {SevereDay, 1, "SPC: Convective outlook, day 1"},
        {SevereDay, 2, "SPC: Convective outlook, day 2"},
        {SevereDay, 3, "SPC: Convective outlook, day 3"},
        {SevereDay, 4, "SPC: Convective outlook, days 4-8"},
        {State, 1, "SPC: Convective outlook, state graphics"},
        {FireSummary, 0, "SPC: Fire weather outlooks, summary"},
        {FireDay, 0, "SPC: Fire weather outlook, days 1-8"},
        {Thunderstorm, 0, "SPC: Thunderstorm outlooks"},
        {NiceDay, 0, "Nice weather outlook, days 1-7 (a work in progress)"},
        {RainfallSummary, 0, "WPC: Excessive rainfall outlooks, summary"},
        {RainfallDay, 0, "WPC: Excessive rainfall outlook, day 1"},
        {RainfallDay, 1, "WPC: Excessive rainfall outlook, day 2"},
        {RainfallDay, 2, "WPC: Excessive rainfall outlook, day 3"},
        {RainfallDay, 3, "WPC: Excessive rainfall outlook, day 4"},
        {RainfallDay, 4, "WPC: Excessive rainfall outlook, day 5"},
        {StormReports, 0, "SPC: Storm reports, today"},
        {StormReports, 1, "SPC: Storm reports, yesterday"},
        {Mesoanalysis, 0, "SPC: Mesoanalysis"},
        {Comparison, 0, "SPC: Severe outlook comparison (SPC, CSU-MLP, CIPS)"},
        {CompositeMap, 0, "SPC: Composite map"},
        {Dashboard, 0, "SPC: Severe dashboard"},
        {NationalText, 0, "WPC: National forecast text"},
        {NationalImages, 0, "WPC: National forecast images"},
        {NhcProducts, 0, "NHC: Product viewer"},
        {Hurricanes, 0, "NHC: Tropical cyclones (Atlantic, East and Central Pacific)"},
        {TropicalHub, 0, "NHC: Tropical Hub"},
    };
    return all;
}

bool OutlookNav::isPage(Kind kind) {
    return kind == SevereSummary || kind == SevereDay || kind == State || kind == FireSummary || kind == FireDay || kind == NiceDay || kind == Thunderstorm || kind == RainfallSummary || kind == RainfallDay || kind == NationalImages;
}

OutlookNav::OutlookNav(Window * owner, Kind kind, int value)
    : owner{owner}
    , comboBox{owner}
{
    vector<string> labels{"Go to another outlook..."};
    items.push_back({SevereSummary, 0});   // the prompt: nothing opens
    for (const auto& d : destinations()) {
        // the screen itself is left out, and the pages of the outlook window are not listed in a screen that is not one (they have the window's own list)
        if ((d.kind == kind && d.value == value) || (d.kind == FireDay && kind == FireDay) || (d.kind == NiceDay && kind == NiceDay)) {
            continue;
        }
        items.push_back({d.kind, d.value});
        labels.push_back(d.label);
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
    open(owner, item.kind, item.value);
    comboBox.block();   // the list shows its prompt again: this screen stays, the choice opened beside it
    comboBox.setIndex(0);
    comboBox.unblock();
}

void OutlookNav::open(Window * from, Kind kind, int value) {
    if (isPage(kind)) {
        OutlookScreen::open(from, kind, value);
        return;
    }
    auto * parent = dynamic_cast<Window *>(from->parentWidget());
    if (parent == nullptr) {
        parent = from;
    }
    switch (kind) {
        case StormReports: new SpcStormReports{parent, value == 0 ? "today" : "yesterday"}; break;
        case Mesoanalysis: new SpcMeso{parent}; break;
        case Comparison: new SvrComparison{parent}; break;
        case CompositeMap: new SpcCompMap{parent}; break;
        case Dashboard: new SevereDashboard{parent}; break;
        case NationalText: new ::NationalText{parent}; break;
        case NhcProducts: new Nhc{parent}; break;
        case Hurricanes: new HurricaneViewer{parent}; break;
        case TropicalHub: Dashboards::openTropicalHub(parent); break;
        default: break;
    }
}
