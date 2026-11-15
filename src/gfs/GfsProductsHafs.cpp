// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The hurricane model HAFS: the storm's own charts.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void hafsProducts(std::vector<Product>& p, const char * model) {
    const auto hurricaneLines = [] {
        auto c = pressure();
        c.highsAndLows = false;
        return c;
    };
    const auto make = [model] (const char * id, const char * label) {
        Product x;
        x.source = model;
        x.id = id;
        x.label = label;
        return x;
    };
    {
        auto x = make("wind_mslp", "10m Wind, MSLP and Barbs");
        x.wants = {want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
        x.fill = speedOf("u", "v");
        x.ramp = tropicalWind();
        x.fillTitle = "10 m wind speed (kt)";
        x.legendStep = 0;
        x.contours = {hurricaneLines()};
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        auto x = make("reflectivity", "Simulated Radar (Composite Reflectivity) and MSLP");
        x.wants = {want("r", "REFC", "entire atmosphere (considered as a single layer)"), want("p", "PRMSL", "mean sea level")};
        x.fill = [] (const Grids& g) { return pick(g, "r"); };
        x.ramp = reflectivity();
        x.fillTitle = "Composite reflectivity (dBZ)";
        x.legendStep = 10;
        x.contours = {hurricaneLines()};
        p.push_back(x);
    }
    // the simulated satellite channels (the file names them only by number): 65 is a window channel (the surface shows through; the cold cloud tops in color), 53 / 54 / 55 are
    // water vapor channels from the upper level down
    for (const auto& [band, id, label, vapor] : {std::tuple{65, "sat_ir", "Simulated Satellite: Infrared (window)", false}, {53, "sat_wv_upper", "Simulated Satellite: Water Vapor, upper level", true},
                                                 {54, "sat_wv_mid", "Simulated Satellite: Water Vapor, middle level", true}, {55, "sat_wv_low", "Simulated Satellite: Water Vapor, lower level", true}}) {
        auto x = make(id, label);
        x.wants = {want("s", ("var discipline=3 center=7 local_table=1 parmcat=192 parm=" + std::to_string(band)).c_str(), "top of atmosphere")};
        x.fill = [] (const Grids& g) { return pick(g, "s"); };
        x.ramp = vapor ? waterVapor() : satelliteIr();
        x.fillTitle = "Brightness temperature (K)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("rain_total", "Rainfall since the start of the run and MSLP");
        x.wants = {GfsData::Want{"a", "APCP", "surface", "0-*", ""}, want("p", "PRMSL", "mean sea level")};
        x.fill = [] (const Grids& g) { return pick(g, "a"); };
        x.ramp = precipitation();
    x.palette = "precip";
        x.quantity = Quantity::Millimeters;
        x.fillTitle = "Rainfall since the start of the run";
        x.legendStep = 0;
        x.contours = {hurricaneLines()};
        p.push_back(x);
    }
    {
        auto x = make("sst", "Sea Surface Temperature and MSLP");
        x.wants = {want("t", "WTMP", "surface"), want("p", "PRMSL", "mean sea level")};
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = seaSurfaceTemperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "Sea surface temperature";
        x.legendStep = 2;
        x.contours = {hurricaneLines()};
        p.push_back(x);
    }
    for (const char * id : {"shear_850_200", "850_vort_ht"}) {
        for (const auto& candidate : p) {
            if (candidate.id == id && candidate.source == "GFS") {
                auto x = candidate;
                x.source = model;
                p.push_back(std::move(x));
                break;
            }
        }
    }
    {   // the swaths: the strongest wind (and gust) of the run so far, and the strongest rotation in each 3 hours (from the file of the whole parent domain)
        const auto window = [] (int hour) { return hour % 24 == 0 ? "0-" + std::to_string(hour / 24) + " day max fcst" : "0-" + std::to_string(hour) + " hour max fcst"; };
        const auto swath = [&] (const char * id, const char * label, const char * variable, const char * title) {
            auto x = make(id, label);
            x.needs = [window, variable] (int hour) {
                return std::vector<GfsData::Need>{{hour, {"w", variable, "10-10 m above ground", window(hour), "", "swath"}}};
            };
            x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "w"), msToKnots); };
            x.ramp = tropicalWind();
            x.fillTitle = title;
            x.legendStep = 0;
            return x;
        };
        p.push_back(swath("swath_wind", "Swath: Strongest 10m Wind so far", "WIND", "Strongest 10 m wind since the start of the run (kt)"));
        p.push_back(swath("swath_gust", "Swath: Strongest 10m Gust so far", "GUST", "Strongest 10 m gust since the start of the run (kt)"));
    }
    {   // the wave file holds every hour: the record says which ("anl", then "3 hour fcst" ...)
        auto x = make("waves", "Significant Wave Height, Peak Period and Wind");
        x.needs = [] (int hour) {
            const std::string when = hour == 0 ? "anl" : std::to_string(hour) + " hour fcst";
            return std::vector<GfsData::Need>{{hour, {"h", "HTSGW", "surface", when, "", "ww3"}}, {hour, {"t", "PERPW", "surface", when, "", "ww3"}}, {hour, {"u", "UGRD", "surface", when, "", "ww3"}},
                                              {hour, {"v", "VGRD", "surface", when, "", "ww3"}}};
        };
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = waveHeight();
        x.fillTitle = "Significant wave height (m)";
        x.legendStep = 0;
        ContourSet period;
        period.key = "t";
        period.interval = 2;
        period.title = "Peak period (s)";
        period.color = QColor{255, 255, 255};
        x.contours = {period};
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
