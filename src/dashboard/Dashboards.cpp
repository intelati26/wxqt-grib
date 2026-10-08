// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "dashboard/Dashboards.h"
#include <string>
#include <vector>
#include "climate/ClimateViewer.h"
#include "dashboard/PlaceholderDashboard.h"
#include "misc/ImageViewer.h"
#include "misc/TextViewerStatic.h"
#include "nhc/Nhc.h"
#include "objects/FutureText.h"
#include "dashboard/TropicalHub.h"
#include "space/SpaceWeatherViewer.h"
#include "tornado/TornadoViewer.h"
#include "hurricane/HurricaneViewer.h"
#include "tropical/TropicalViewer.h"

namespace {
    using Panel = PlaceholderDashboard::Panel;
    using Link = PlaceholderDashboard::Link;

    // a text product in a window of its own
    Link textLink(const string& label, const string& url) {
        return Link{label, [label, url] (Window * parent) {
            new FutureText{parent, url, [parent, label] (const string& text) {
                new TextViewerStatic{parent, text.empty() ? label + " is not available right now." : text, label, 800, 700};
            }};
        }};
    }
}

void Dashboards::openTropicalHub(Window * parent) {
    new TropicalHub{parent};
}

void Dashboards::openSpaceWeather(Window * parent) {
    new SpaceWeatherViewer{parent};
}

void Dashboards::openTornadoHistory(Window * parent) {
    new TornadoViewer{parent};
}

void Dashboards::openForecastDiscussions(Window * parent) {
    new PlaceholderDashboard{parent, "Forecast discussions",
        "Every forecast discussion in one place - area forecast discussions, NHC, WPC, SPC, CPC and space weather - with a history of past ones and a "
        "view that compares a discussion with the previous one. Most of these are already in the app, spread over several menus.",
        {
            Panel{"Area forecast discussions", "Any office, newest first, with a search box.",
                "NWS API / text files (already used by the app)", "Step 1"},
            Panel{"Tropical", "NHC storm discussions and the tropical weather discussions for the Atlantic and Pacific.",
                "NHC text products (already used)", "Step 1"},
            Panel{"National centres", "WPC short-range, extended, Alaska, Hawaii, tropical, excessive rainfall and heavy snow discussions; SPC mesoscale discussions.",
                "WPC / SPC (already used)", "Step 1"},
            Panel{"Climate and space", "The CPC ENSO diagnostic discussion and the SWPC forecast discussion.",
                "CPC / SWPC (already used)", "Step 1"},
            Panel{"History", "Past discussions for any office or product, by date.",
                "Iowa Environmental Mesonet AFOS text archive", "Step 2"},
            Panel{"What changed", "A discussion shown beside the previous one with the changed lines marked.",
                "Two issuances from the archive", "Step 2"},
        },
        {
            textLink("Space weather discussion (existing)", "https://services.swpc.noaa.gov/text/discussion.txt"),
            Link{"NHC tool (existing)", [] (Window * w) { new Nhc{w}; }},
            Link{"Climate and ocean: ENSO discussion (existing)", [] (Window * w) { new ClimateViewer{w}; }},
        }};
}
