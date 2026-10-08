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
    new PlaceholderDashboard{parent, "Tornado history",
        "Past tornado tracks and statistics from the Storm Prediction Center's tornado database (1950 to the present, one row per tornado). "
        "Nothing here is built yet; the data was checked and the graphs were prototyped from the real file.",
        {
            Panel{"Track map", "Tornadoes for a year range, state or area as lines from start to end point (a dot when there is no end point), coloured by rating; click for the details.",
                "SPC 1950-2025 tornado CSV (or the SVRGIS shapefiles)", "Step 1"},
            Panel{"Tornadoes per day over the year", "Bars for each day of a chosen year with the average of a chosen span of years as a line; variants: cumulative count against the average.",
                "SPC tornado CSV", "Step 1"},
            Panel{"Climatology heatmap", "Years down, weeks across, colour = tornadoes that week (log scale), with the 1991-2020 mean row; filters for state, rating, fatal only.",
                "SPC tornado CSV", "Step 1"},
            Panel{"Filters", "Years, state, minimum rating, deaths, and 'within N km of my location'.",
                "SPC tornado CSV", "Step 1"},
            Panel{"Counts by year, month and rating", "Bar charts of tornadoes and deaths, using the existing bar-chart widget.",
                "SPC tornado CSV", "Step 1"},
            Panel{"Outbreak days", "The days with the most tornadoes or deaths; open one for its map and reports.",
                "SPC tornado CSV; SPC daily report files", "Step 2"},
            Panel{"Open the radar for a tornado", "Jump to the radar at the nearest site and time of a chosen tornado, with that day's warnings from the archive.",
                "Radar history (already built); IEM warnings archive", "Step 2"},
            Panel{"Damage surveys", "Surveyed paths and damage points for recent events.",
                "NWS Damage Assessment Toolkit service (preliminary data)", "Later"},
        }};
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
