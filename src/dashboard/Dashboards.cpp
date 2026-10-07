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
    new PlaceholderDashboard{parent, "Tropical Hub",
        "One place for current storms, past tracks, seasons, model guidance, aircraft reconnaissance and the ocean around storms. "
        "Today's Tropical screen (active storms worldwide) and the Climate and ocean screen already exist; this hub will gather them with the new panels.",
        {
            Panel{"Active storms", "A card for each active storm (name, class, wind, pressure, position, movement, advisory time) with a small satellite picture; click for the storm page.",
                "NHC CurrentStorms.json; CIRA / RAMMB for satellite pictures", "Phase 1"},
            Panel{"Track map", "Best track so far, the official forecast line and cone, and the model tracks as thin lines with a check-list to switch them; coloured by category.",
                "NHC ATCF files (btk, fst, aid_public); NHC GIS cone", "Phase 1"},
            Panel{"Season table and ACE", "Storms, hurricanes, major hurricanes and ACE for the year by basin, and ACE across the years.",
                "NHC best-track and HURDAT2 files", "Phase 1"},
            Panel{"Storm page", "Wind and pressure history with the official and SHIPS forecasts, wind radii by quadrant, environment (shear, SST, humidity), parameters table.",
                "ATCF best track and forecast; SHIPS text", "Phase 2"},
            Panel{"Historical tracks", "Pick a basin and year or a storm name; filter by category; storms that passed within N km of a point.",
                "IBTrACS since-1980 file; HURDAT2", "Phase 3"},
            Panel{"Recon: pending missions", "The Tropical Cyclone Plan of the Day: planned and requested flights (aircraft, storm, times, target point), or 'no flights planned'.",
                "NHC REPRPD in the recon archive", "Recon step"},
            Panel{"Recon: flight data", "Mission list, flight track coloured by wind, pressure / wind / temperature strips and vortex-message table from the high-density observations.",
                "NHC recon archive (HDOB, vortex messages)", "Recon step"},
            Panel{"SST and ocean", "Sea surface temperature and anomaly, heat content and 20 C depth, regional time series; a 'SST around this storm' shortcut.",
                "The Climate and ocean screen; ocean heat source still open", "Phase 4"},
            Panel{"Models", "Tropical model fields: winds, shear, vorticity, moisture, with a forecast-hour slider.",
                "GFS / GEFS (NOMADS); needs the multi-model decision", "Later"},
        },
        {
            Link{"Tropical cyclones: track, spaghetti, recon (new)", [] (Window * w) { new HurricaneViewer{w}; }},
            Link{"Tropical: active storms (existing)", [] (Window * w) { new TropicalViewer{w}; }},
            Link{"Climate and ocean (existing)", [] (Window * w) { new ClimateViewer{w}; }},
            Link{"NHC tool (existing)", [] (Window * w) { new Nhc{w}; }},
        }};
}

void Dashboards::openSpaceWeather(Window * parent) {
    new PlaceholderDashboard{parent, "Space weather",
        "Sun, solar wind and Earth's magnetic field from NOAA's Space Weather Prediction Center (SWPC): storm scales, the Kp index, aurora, flares and "
        "sun pictures. The aurora forecast pictures and the SWPC text products are already in the app; the buttons open them.",
        {
            Panel{"Status strip", "The current R / S / G storm scales (radio blackouts, radiation, geomagnetic), today's and the forecast, with the current Kp and any active alerts.",
                "SWPC noaa-scales.json, alerts.json", "Step 1"},
            Panel{"Kp index", "Bar chart of the planetary K index with its forecast; the Dst index beside it.",
                "SWPC noaa-planetary-k-index(-forecast).json, kyoto-dst.json", "Step 1"},
            Panel{"Aurora", "Our own aurora map drawn from the gridded forecast, north and south, instead of only the picture.",
                "SWPC json/ovation_aurora_latest.json", "Step 2"},
            Panel{"Solar flares", "GOES X-ray flux over the last day or longer, with the flare class lines.",
                "SWPC json/goes/primary/xrays-1-day.json", "Step 1"},
            Panel{"Sun and corona", "SUVI pictures in several wavelengths and the LASCO coronagraph, with loops; zoom and save like the other pictures.",
                "SWPC images/animations and products/animations", "Step 2"},
            Panel{"Solar wind", "Speed, density and magnetic field near Earth.",
                "SWPC - the address is not found yet (the paths tried returned 404)", "Open"},
            Panel{"Forecast and outlook text", "The 3-day forecast, geomagnetic forecast, 27-day outlook, discussion and advisory outlook in one place.",
                "SWPC text products (already used by the app)", "Step 1"},
            Panel{"Solar cycle", "Sunspot number and 10.7 cm flux with the cycle 25 predicted range.",
                "SWPC solar-cycle-25 JSON files", "Later"},
        },
        {
            Link{"Aurora forecast - north (existing)", [] (Window * w) { new ImageViewer{w, string{"https://services.swpc.noaa.gov/images/animations/ovation/north/latest.jpg"}, "Aurora forecast - north"}; }},
            textLink("Space weather discussion", "https://services.swpc.noaa.gov/text/discussion.txt"),
            textLink("3-day forecast", "https://services.swpc.noaa.gov/text/3-day-forecast.txt"),
        }};
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
