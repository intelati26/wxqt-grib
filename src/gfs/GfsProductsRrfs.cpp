// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The RRFS, the HRRR and the MPAS runs: the rapid-refresh family of convection-allowing models.
#include "gfs/GfsChartInternal.h"
#include <QTimeZone>
#include <cmath>
#include <limits>

namespace GfsChart {
namespace detail {

void rrfsProducts(std::vector<Product>& p, const std::string& model) {
    const bool hrrr = model.compare(0, 4, "HRRR") == 0;
    const auto make = [&model] (const char * id, const char * label) {
        Product x;
        x.source = model;
        x.id = id;
        x.label = label;
        return x;
    };
    const auto scaledOf = [] (const char * key, double factor) { return [key, factor] (const Grids& g) { return GfsGrid::scaled(pick(g, key), factor); }; };
    const auto sea = [hrrr] { return want("p", hrrr ? "MSLMA" : "MSLET", "mean sea level"); };
    const auto wind10 = [&] (Product& x) {
        x.wants.push_back(want("u", "UGRD", "10 m above ground"));
        x.wants.push_back(want("v", "VGRD", "10 m above ground"));
        x.barbU = "u";
        x.barbV = "v";
    };
    const auto hourly = [] (int hour) { return std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst"; };   // the last hour's maximum
    {
        auto x = make("10m_wnd", "10m Wind and MSLP");
        wind10(x);
        x.wants.push_back(sea());
        x.fill = speedOf("u", "v");
        x.ramp = lowWind();
        x.fillTitle = "10 m wind speed (kt)";
        x.legendStep = 10;
        x.contours = {pressure()};
        p.push_back(x);
    }
    {
        auto x = make("10m_wnd_sfc_gust", "10m Wind Barbs and Surface Gust");
        wind10(x);
        x.wants.push_back(want("g", "GUST", "surface"));
        x.fill = scaledOf("g", msToKnots);
        x.ramp = lowWind();
        x.fillTitle = "Surface wind gust (kt)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("10m_maxwnd", "Maximum 10m Wind in the last hour");
        x.needs = [hourly] (int hour) {
            return std::vector<GfsData::Need>{{hour, {"u", "MAXUW", "10 m above ground", hourly(hour), ""}}, {hour, {"v", "MAXVW", "10 m above ground", hourly(hour), ""}}};
        };
        x.fill = speedOf("u", "v");
        x.ramp = lowWind();
        x.fillTitle = "Strongest 10 m wind of the last hour (kt)";
        x.legendStep = 10;
        p.push_back(x);
    }
    for (const auto& [id, label, layer] : {std::tuple{"sfc_cape_cin", "Surface-Based CAPE and CIN", "surface"}, {"best_cape_cin", "Most Unstable CAPE and CIN", "255-0 mb above ground"}}) {
        auto x = make(id, label);
        x.wants = {want("c", "CAPE", layer), want("n", "CIN", layer)};
        x.fill = [] (const Grids& g) { return pick(g, "c"); };
        x.ramp = capeRamp();
        x.fillTitle = "CAPE (J/kg)";
        x.legendStep = 500;
        ContourSet cin;
        cin.key = "n";
        cin.interval = 100;
        cin.title = "CIN (J/kg)";
        cin.color = QColor{70, 70, 70};
        cin.dashed = true;
        x.contours = {cin};
        p.push_back(x);
    }
    {   // the files' "best 4-layer lifted index" of the lowest 180 mb (their surface-based record, LFTX 500-1000 mb, holds temperatures of 260-290 K, not an index: it is not used)
        auto x = make("lifted_index_ml", "Best Lifted Index (lowest 180 mb)");
        x.wants = {want("l", "4LFTX", "180-0 mb above ground")};
        x.fill = [] (const Grids& g) { return pick(g, "l"); };
        x.ramp = Ramp{{{-14, QColor{"#6a2c9a"}}, {-10, QColor{"#d02fb0"}}, {-8, QColor{"#e0301e"}}, {-6, QColor{"#f07a1a"}}, {-4, QColor{"#f5c842"}}, {-2, QColor{"#f6f0a0"}}, {0, QColor{"#e8f0e8"}}, {2, QColor{232, 240, 232, 0}}}};
        x.fillTitle = "Best (4-layer) lifted index of the lowest 180 mb (negative: unstable)";
        x.legendStep = 2;
        p.push_back(x);
    }
    for (const auto& [id, label, layer] : {std::tuple{"helicity_1km", "0-1 km Storm-Relative Helicity", "1000-0 m above ground"}, {"helicity_3km", "0-3 km Storm-Relative Helicity", "3000-0 m above ground"}}) {
        auto x = make(id, label);
        x.wants = {want("h", "HLCY", layer)};
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = helicityRamp();
        x.palette = "helicity";
        x.fillTitle = "Storm-relative helicity (m2/s2)";
        x.legendStep = 100;
        p.push_back(x);
    }
    {
        auto x = make("max_updraft_hlcy", "Maximum 2-5 km Updraft Helicity in the last hour");
        x.needs = [hourly] (int hour) { return std::vector<GfsData::Need>{{hour, {"h", "MXUPHL", "5000-2000 m above ground", hourly(hour), ""}}}; };
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = updraftHelicity();
        x.palette = "uh";
        x.fillTitle = "Updraft helicity (m2/s2)";
        x.legendStep = 0;
        p.push_back(x);
    }
    // the strongest relative vorticity of the last hour in the lowest 1 km and 2 km: the low-level rotation (the helicity above is the 2-5 km mid-level one), in 1e-3 per second
    for (const auto& [id, label, layer] : {std::tuple{"max_relv_0_1km", "Maximum 0-1 km Relative Vorticity in the last hour", "1000-0 m above ground"},
                                            {"max_relv_0_2km", "Maximum 0-2 km Relative Vorticity in the last hour", "2000-0 m above ground"}}) {
        auto x = make(id, label);
        x.needs = [hourly, layer = std::string{layer}] (int hour) { return std::vector<GfsData::Need>{{hour, {"v", "RELV", layer, hourly(hour), ""}}}; };
        x.derive = [] (Grids& g, const Context&) {
            for (auto& value : g.at("v").values) {
                value *= 1000.0f;
            }
        };
        x.fill = [] (const Grids& g) { return pick(g, "v"); };
        x.ramp = lowLevelVorticity();
        x.fillTitle = "Relative vorticity (1e-3 /s)";
        x.legendStep = 0;
        p.push_back(x);
    }
    for (auto& chart : p) {   // the composite reflectivity (the GFS chart cloned above) with the rotating storms outlined: the last hour's strongest 2-5 km updraft helicity, 40 and over
        if (chart.source != model || chart.id != "sim_radar_comp") {
            continue;
        }
        chart.wants.clear();
        chart.needs = [hourly, hrrr] (int hour) {   // the RRFS files call the whole-column level by its long name
            std::vector<GfsData::Need> out;
            out.push_back({hour, {"r", "REFC", hrrr ? "entire atmosphere" : "entire atmosphere (considered as a single layer)", "", ""}});
            out.push_back({hour, {"uh", "MXUPHL", "5000-2000 m above ground", hourly(hour), ""}});
            return out;
        };
        chart.fallbackNeeds = [hrrr] (int hour) {   // a run with no hourly maximum of the helicity (the RRFS runs that only have subhourly files): the echoes alone, no outlines
            return std::vector<GfsData::Need>{{hour, {"r", "REFC", hrrr ? "entire atmosphere" : "entire atmosphere (considered as a single layer)", "", ""}}};
        };
        chart.derive = [] (Grids& g, const Context&) {
            if (g.find("uh") == g.end()) {   // the start of the run has no hour's maximum yet
                g["uh"] = g.at("r");
                std::fill(g["uh"].values.begin(), g["uh"].values.end(), 0.0f);
            }
        };
        ContourSet uh;
        uh.key = "uh";
        uh.interval = 40;
        uh.base = 40;
        uh.minimum = 39;
        uh.title = "2-5 km updraft helicity of the last hour, 40 and over (m2/s2)";
        uh.color = QColor{10, 10, 10};
        uh.width = 2.5;
        uh.uniform = true;
        chart.contours = {uh};
        chart.label = "Composite Reflectivity with Updraft Helicity of 40 or more";
        break;
    }
    {
        // MAG colors the echoes by what is falling: green rain, blue snow, purple sleet, red freezing rain (the type flags are the model's, at the hour shown)
        const auto shades = [] (QColor light, QColor dark) {
            Ramp r;
            r.stops.push_back({0.0, QColor{light.red(), light.green(), light.blue(), 0}});
            r.stops.push_back({4.9, QColor{light.red(), light.green(), light.blue(), 0}});
            const double dbz[] = {5, 15, 25, 35, 45, 55, 65, 75};
            for (int i = 0; i < 8; i++) {
                const double f = i / 7.0;
                r.stops.push_back({dbz[i], QColor{static_cast<int>(light.red() + (dark.red() - light.red()) * f), static_cast<int>(light.green() + (dark.green() - light.green()) * f),
                                                 static_cast<int>(light.blue() + (dark.blue() - light.blue()) * f)}});
            }
            return r;
        };
        auto x = make("sim_radar_1km", "Simulated Radar at 1 km");
        x.wants = {want("r", "REFD", "1000 m above ground"), want("cr", "CRAIN", "surface"), want("cs", "CSNOW", "surface"), want("cf", "CFRZR", "surface"), want("ci", "CICEP", "surface")};
        x.derive = [] (Grids& g, const Context&) {
            auto rain = g["r"], snow = g["r"], sleet = g["r"], freezing = g["r"];
            for (size_t i = 0; i < rain.values.size(); i++) {
                const bool isSnow = g["cs"].values[i] >= 0.5f, isFreezing = g["cf"].values[i] >= 0.5f, isSleet = g["ci"].values[i] >= 0.5f;
                const float z = g["r"].values[i], none = std::nanf("");
                rain.values[i] = !isSnow && !isFreezing && !isSleet ? z : none;
                snow.values[i] = isSnow && !isFreezing && !isSleet ? z : none;
                sleet.values[i] = isSleet && !isFreezing ? z : none;
                freezing.values[i] = isFreezing ? z : none;
            }
            g["rain"] = rain;
            g["snow"] = snow;
            g["sleet"] = sleet;
            g["freezing"] = freezing;
        };
        x.fill = [] (const Grids& g) { return pick(g, "rain"); };
        x.ramp = Ramp{{{0, QColor{160, 255, 0, 0}}, {4.9, QColor{160, 255, 0, 0}}, {5, QColor{160, 255, 0}}, {15, QColor{80, 220, 0}}, {25, QColor{0, 170, 0}}, {35, QColor{255, 230, 0}}, {45, QColor{255, 150, 0}},
                        {55, QColor{230, 0, 0}}, {65, QColor{255, 170, 170}}, {75, QColor{160, 60, 220}}}};   // the rain scale of the model guidance site: greens, then yellow, orange, red, pink and purple for the heaviest
        x.palette = "radar_rain";
        x.overlays = {{"snow", shades(QColor{205, 225, 255}, QColor{0, 0, 150})}, {"sleet", shades(QColor{235, 205, 255}, QColor{95, 0, 135})}, {"freezing", shades(QColor{255, 205, 205}, QColor{150, 0, 0})}};
        x.fillTitle = "Reflectivity at 1 km (dBZ): green rain, blue snow, purple sleet, red freezing rain";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("sim_radar_max", "Maximum Simulated Radar of the last hour");
        x.needs = [hourly] (int hour) { return std::vector<GfsData::Need>{{hour, {"r", "MAXREF", "1000 m above ground", hourly(hour), ""}}}; };
        x.fill = [] (const Grids& g) { return pick(g, "r"); };
        x.ramp = reflectivity();
        x.fillTitle = "Maximum reflectivity of the last hour (dBZ)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("echo_top", "Echo Top");
        x.wants = {hrrr ? want("t", "RETOP", "cloud top") : want("t", "RETOP", "entire atmosphere (considered as a single layer)")};
        x.fill = scaledOf("t", 3.28084 / 1000.0);
        x.ramp = echoTopRamp();
        x.palette = "echo_top";
        x.fillTitle = "Echo top (thousands of feet)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("ceiling", "Cloud Ceiling");
        x.wants = {hrrr ? want("c", "HGT", "cloud ceiling") : want("c", "CEIL", "cloud ceiling")};
        x.fill = scaledOf("c", 3.28084);
        x.ramp = ceilingRamp();
        x.palette = "ceiling";
        x.fillTitle = "Ceiling (feet)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = make("vis", "Surface Visibility");
        x.wants = {want("v", "VIS", "surface")};
        x.fill = scaledOf("v", 1.0 / 1609.344);
        x.ramp = visibilityRamp();
        x.palette = "vis";
        x.fillTitle = "Visibility (miles)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = make("lightning", "Lightning (flashes in the last hour)");
        x.needs = [hourly, hrrr] (int hour) { return std::vector<GfsData::Need>{{hour, {"l", "LTNG", "entire atmosphere", hrrr ? std::string{} : hourly(hour), ""}}}; };
        x.fill = [] (const Grids& g) { return pick(g, "l"); };
        x.ramp = lightningRamp();
        x.fillTitle = "Lightning";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = make("snow_total", "Total Snowfall since the start of the run");
        x.needs = [] (int hour) { return std::vector<GfsData::Need>{{hour, {"s", "ASNOW", "surface", "0-*", ""}}}; };
        x.fill = scaledOf("s", 100.0);   // meters -> centimeters
        x.ramp = snowfall();
        x.quantity = Quantity::Centimeters;
        x.fillTitle = "Snowfall since the start of the run";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = make("precip_rate", "Precipitation Rate");
        x.wants = {want("r", "PRATE", "surface")};
        x.fill = scaledOf("r", 3600.0);   // mm per second -> mm per hour
        x.ramp = precipitation();
    x.palette = "precip";
        x.quantity = Quantity::Millimeters;
        x.fillTitle = "Precipitation rate (per hour)";
        x.legendStep = 0;
        p.push_back(x);
    }
    if (model.compare(0, 4, "RRFS") == 0 || model.compare(0, 4, "HRRR") == 0) {   // the clouds by height (low, middle, high) in one map: the RRFS and the HRRR and their regional runs (checked 16 Nov 2026)
        p.push_back(cloudLayersProduct(model));
    }
    if (model == "RRFS") {   // the storm surface runoff (SSRUN): the water that runs off the ground instead of soaking in, an amount for each hour in each file (kg/m2 = mm): where the rain is
        // more than the ground takes, the flash flood guidance of the model itself
        const auto runoff = [] (int hour) { return std::to_string(hour - 1) + "-" + std::to_string(hour) + " hour acc fcst"; };
        for (const int span : {1, 6}) {
            auto x = make(span == 1 ? "runoff_1hr" : "runoff_6hr", span == 1 ? "Surface Runoff in the last hour" : "Surface Runoff in the last 6 hours");
            x.needs = [runoff, span] (int hour) {
                std::vector<GfsData::Need> needs;
                for (int h = std::max(1, hour - span + 1); h <= hour; h++) {
                    needs.push_back({h, {"r" + std::to_string(h), "SSRUN", "surface", runoff(h), ""}});
                }
                return needs;
            };
            x.derive = [span] (Grids& g, const Context& context) {   // the hours' amounts added up (from hour 1 when the run is younger than the span)
                GfsGrid::Grid total;
                for (int h = std::max(1, context.hour - span + 1); h <= context.hour; h++) {
                    const auto found = g.find("r" + std::to_string(h));
                    if (found == g.end() || found->second.empty()) {
                        continue;
                    }
                    if (total.empty()) {
                        total = found->second;
                    } else {
                        for (size_t i = 0; i < total.values.size() && i < found->second.values.size(); i++) {
                            total.values[i] += found->second.values[i];
                        }
                    }
                }
                for (auto& v : total.values) {   // (no runoff: clear, so the map under it shows)
                    v = v > 0.05f ? v : std::numeric_limits<float>::quiet_NaN();
                }
                g["runoff"] = std::move(total);
            };
            x.fill = [] (const Grids& g) { return pick(g, "runoff"); };
            x.ramp = precipitation();
            x.palette = "precip";
            x.quantity = Quantity::Millimeters;
            x.fillTitle = span == 1 ? "Water running off the ground in the last hour (the rain the ground does not take)" : "Water running off the ground in the last 6 hours (the rain the ground does not take)";
            x.legendStep = 0;
            p.push_back(x);
        }
    }
    {
        auto x = make("500_temp_ht", "500mb Temperature and Height");
        x.wants = {want("t", "TMP", "500 mb"), want("z", "HGT", "500 mb"), want("u", "UGRD", "500 mb"), want("v", "VGRD", "500 mb")};
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "500 mb temperature";
        x.legendStep = 5;
        x.contours = {heights(6)};
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        auto x = make("925_temp_wnd", "925mb Temperature and Wind");
        x.wants = {want("t", "TMP", "925 mb"), want("u", "UGRD", "925 mb"), want("v", "VGRD", "925 mb")};
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "925 mb temperature";
        x.legendStep = 5;
        // the model guidance site draws no height lines here: the temperature and the wind only
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    // the maps the other convection-allowing models have (HRRR, NAM nests, HiResW, RAP) that were not drawn yet
    for (const auto& [id, label, variable, ramp, title] : {std::tuple{"2m_temp_10m_wnd", "2m Temperature and 10m Wind", "TMP", 0, "2 m temperature"}, {"2m_dewp_10m_wnd", "2m Dew Point and 10m Wind", "DPT", 1, "2 m dew point"}}) {
        auto x = make(id, label);
        x.wants = {want("t", variable, "2 m above ground")};
        wind10(x);
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = ramp == 0 ? temperature() : dewpoint();
        x.quantity = Quantity::Temperature;
        x.fillTitle = title;
        x.legendStep = 5;
        p.push_back(x);
    }
    {   // 700 mb humidity with the rising air as lines: the model's vertical velocity in m/s changed to omega in Pa/s with the density of air at 700 mb (about 0.9 kg per cubic meter)
        auto x = make("700_rh_ht", "700mb Relative Humidity, Height and Omega");
        x.wants = {want("z", "HGT", "700 mb"), want("rh", "RH", "700 mb"), want("w", hrrr ? "VVEL" : "DZDT", "700 mb"), want("u", "UGRD", "700 mb"), want("v", "VGRD", "700 mb")};
        x.barbU = "u";
        x.barbV = "v";
        x.derive = [hrrr] (Grids& g, const Context&) { g["o"] = GfsGrid::scaled(g["w"], hrrr ? 1.0 : -0.9 * 9.80665); };   // (the HRRR gives omega itself, in Pa/s)
        x.fill = [] (const Grids& g) { return pick(g, "rh"); };
        x.ramp = humidity();
        x.fillTitle = "700 mb relative humidity (%)";
        x.legendStep = 10;
        ContourSet omega;
        omega.key = "o";
        omega.scale = 10.0;
        omega.interval = 2;
        omega.title = "Omega (ubar/s, rising air)";
        omega.color = QColor{20, 110, 70};
        omega.dashed = true;
        omega.onlyBelowZero = true;
        omega.smoothKm = 30.0;
        x.contours = {heights(3), omega};
        p.push_back(x);
    }
    {   // the strongest rotation of the run so far: the hourly maxima of every hour added up by taking their largest
        auto x = make("accu_max_updraft_hlcy", "Accumulated Maximum 2-5 km Updraft Helicity");
        x.needs = [] (int hour) {
            std::vector<GfsData::Need> out;
            for (int k = 1; k <= hour; k++) {
                out.push_back({k, {"u" + std::to_string(k), "MXUPHL", "5000-2000 m above ground", std::to_string(k - 1) + "-" + std::to_string(k) + " hour max fcst", ""}});
            }
            return out;
        };
        x.derive = [] (Grids& g, const Context& context) {
            auto best = g["u1"];
            for (int k = 2; k <= context.hour; k++) {
                const auto& next = g["u" + std::to_string(k)];
                for (size_t i = 0; i < best.values.size() && i < next.values.size(); i++) {
                    if (std::isnan(best.values[i]) || next.values[i] > best.values[i]) {
                        best.values[i] = next.values[i];
                    }
                }
            }
            g["m"] = best;
        };
        x.fill = [] (const Grids& g) { return pick(g, "m"); };
        x.ramp = updraftHelicity();
        x.palette = "uh";
        x.fillTitle = "Strongest updraft helicity since the start of the run (m2/s2)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        auto x = make("helicity", "Helicity and 30m Wind");
        x.wants = {want("h", "HLCY", "3000-0 m above ground"), want("u", "UGRD", "30 m above ground"), want("v", "VGRD", "30 m above ground")};
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = helicityRamp();
        x.palette = "helicity";
        x.fillTitle = "0-3 km storm-relative helicity (m2/s2)";
        x.legendStep = 100;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    // the fire weather set: what the fire weather nest of the model guidance site draws, from the 3 km run (the nest itself is a finer grid, a later refinement)
    for (const auto& [id, label, variable, ramp, title] : {std::tuple{"2m_tmpc", "Shelter (2 m) Temperature", "TMP", 0, "2 m temperature"}, {"2m_dwpc", "Shelter (2 m) Dew Point Temperature", "DPT", 1, "2 m dew point"}}) {
        auto x = make(id, label);
        x.wants = {want("t", variable, "2 m above ground")};
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = ramp == 0 ? temperature() : dewpoint();
        x.quantity = Quantity::Temperature;
        x.fillTitle = title;
        x.legendStep = 5;
        p.push_back(x);
    }
    {
        auto x = make("2m_rh_10m_wnd", "1-hr Minimum Relative Humidity, 10m Wind");
        x.needs = [] (int hour) {
            return std::vector<GfsData::Need>{{hour, {"r", "MINRH", "2 m above ground", std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour min fcst", ""}},
                                              {hour, {"u", "UGRD", "10 m above ground", "", ""}}, {hour, {"v", "VGRD", "10 m above ground", "", ""}}};
        };
        x.fill = [] (const Grids& g) { return pick(g, "r"); };
        x.ramp = humidity();
        x.fillTitle = "Lowest relative humidity of the last hour (%)";
        x.legendStep = 10;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        auto x = make("best_cape", "Best (Most Unstable) CAPE");
        x.wants = {want("c", "CAPE", "255-0 mb above ground")};
        x.fill = [] (const Grids& g) { return pick(g, "c"); };
        x.ramp = capeRamp();
        x.fillTitle = "Most unstable CAPE (J/kg)";
        x.legendStep = 500;
        p.push_back(x);
    }
    // ---- the "Parametric" indices of the old SPC-style screen (IndexViewer), now charts like the rest (so the maximum over a day, the change since an earlier run, the hover and the compare tiles all
    // work on them). Formulas as that screen had them (docs/derived-severe-indices-plan.md).
    {   // SHIP: the significant hail parameter, with the model's own hail size as lines (inches)
        auto x = make("ship", "SHIP (Significant Hail Parameter)");
        x.needs = [=] (int hour) {
            std::vector<GfsData::Need> out;
            if (hour < 1) {
                return out;
            }
            out.push_back({hour, {"hail", "HAIL", "surface", std::to_string(hour - 1) + "-" + std::to_string(hour) + " hour max fcst", ""}});
            for (const auto& w : {want("cape", "CAPE", "180-0 mb above ground"), want("d2", "DPT", "2 m above ground"), want("sp", "PRES", "surface"), want("us", "VUCSH", "0-6000 m above ground"),
                                  want("vs", "VVCSH", "0-6000 m above ground"), want("fz", "HGT", "0C isotherm"), want("terr", "HGT", "surface"), want("t7", "TMP", "700 mb"), want("t5", "TMP", "500 mb"),
                                  want("z7", "HGT", "700 mb"), want("z5", "HGT", "500 mb")}) {
                out.push_back({hour, w});
            }
            return out;
        };
        x.derive = [] (Grids& g, const Context&) {
            auto out = g["cape"];
            for (size_t i = 0; i < out.values.size(); i++) {
                const double cape = g["cape"].values[i], d2 = g["d2"].values[i], sp = g["sp"].values[i], t5 = g["t5"].values[i];
                const double e = 6.112 * std::exp(17.67 * d2 / (d2 + 243.5));
                const double mixr = 622.0 * e / (sp / 100.0 - e);                                                        // g/kg
                const double lr75 = (g["t7"].values[i] - t5) / ((g["z5"].values[i] - g["z7"].values[i]) / 1000.0);      // C/km
                const double shear = std::hypot(g["us"].values[i], g["vs"].values[i]);                                   // m/s
                const double frz = g["fz"].values[i] - g["terr"].values[i];                                             // m above the ground
                double ship = cape * std::clamp(mixr, 11.0, 13.6) * lr75 * (-std::min(t5, -5.5)) * std::clamp(shear, 7.0, 27.0) / 42000000.0;
                ship *= (cape < 1300.0 ? cape / 1300.0 : 1.0) * (lr75 < 5.8 ? lr75 / 5.8 : 1.0) * (frz < 2400.0 ? frz / 2400.0 : 1.0);
                out.values[i] = std::isfinite(ship) ? static_cast<float>(std::max(0.0, ship)) : std::nanf("");
            }
            g["ship"] = std::move(out);
        };
        x.fill = [] (const Grids& g) { return pick(g, "ship"); };
        x.ramp = Ramp{{{0.0, QColor{0, 0, 40, 0}}, {0.25, QColor{0, 0, 40, 0}}, {0.5, QColor{40, 60, 200}}, {1.0, QColor{60, 170, 90}}, {1.5, QColor{230, 220, 60}}, {2.0, QColor{240, 150, 40}},
                       {3.0, QColor{220, 40, 40}}, {4.0, QColor{170, 30, 150}}, {6.0, QColor{255, 255, 255}}}};
        x.fillTitle = "SHIP: under 1 not significant, 1-4 favorable for hail of 2 in or more, over 4 very high (lines: the model's hail size, in)";
        x.legendStep = 1;
        ContourSet hail;
        hail.key = "hail";
        hail.scale = 39.3701;
        hail.base = 0.5;
        hail.interval = 0.5;
        hail.minimum = 0.75;
        hail.title = "Hail size (in)";
        hail.color = QColor{20, 20, 20};
        hail.width = 1.6;
        x.contours = {hail};
        p.push_back(x);
    }
    {   // STP: the significant tornado parameter, fixed layer (Thompson et al. 2003)
        auto x = make("stp", "STP (Significant Tornado Parameter, fixed layer)");
        x.wants = {want("sbcape", "CAPE", "surface"), want("sbcin", "CIN", "surface"), want("srh1", "HLCY", "1000-0 m above ground"), want("us", "VUCSH", "0-6000 m above ground"),
                   want("vs", "VVCSH", "0-6000 m above ground"), want("lcl", "HGT", "level of adiabatic condensation from sfc"), want("terr", "HGT", "surface")};
        x.derive = [] (Grids& g, const Context&) {
            auto out = g["sbcape"];
            for (size_t i = 0; i < out.values.size(); i++) {
                const double lcl = g["lcl"].values[i] - g["terr"].values[i], shear = std::hypot(g["us"].values[i], g["vs"].values[i]), cin = g["sbcin"].values[i];
                const double stp = (g["sbcape"].values[i] / 1500.0) * (lcl < 1000.0 ? 1.0 : (lcl > 2000.0 ? 0.0 : (2000.0 - lcl) / 1000.0)) * (g["srh1"].values[i] / 150.0) *
                    (shear > 30.0 ? 1.5 : (shear < 12.5 ? 0.0 : shear / 20.0)) * (cin > -50.0 ? 1.0 : (cin < -200.0 ? 0.0 : (200.0 + cin) / 150.0));
                out.values[i] = std::isfinite(stp) ? static_cast<float>(std::max(0.0, stp)) : std::nanf("");
            }
            g["stp"] = std::move(out);
        };
        x.fill = [] (const Grids& g) { return pick(g, "stp"); };
        x.ramp = Ramp{{{0.0, QColor{0, 0, 40, 0}}, {0.25, QColor{0, 0, 40, 0}}, {0.5, QColor{40, 60, 200}}, {1.0, QColor{60, 170, 90}}, {2.0, QColor{230, 220, 60}}, {3.0, QColor{240, 150, 40}},
                       {5.0, QColor{220, 40, 40}}, {8.0, QColor{170, 30, 150}}, {12.0, QColor{255, 255, 255}}}};
        x.fillTitle = "STP: over 1, most significant tornadoes with a supercell; under 1, most non-tornadic ones";
        x.legendStep = 1;
        p.push_back(x);
    }
    {   // the Haines index (mid level, 850-700 mb): stability from the temperature drop between the levels (1 under 6 C, 2 up to 10, 3 above), moisture from the dew point depression at 850 mb (1 under 6 C, 2 up to 12, 3 above)
        auto x = make("haines", "Haines Index (850-700 mb)");
        x.wants = {want("t8", "TMP", "850 mb"), want("t7", "TMP", "700 mb"), want("d8", "DPT", "850 mb")};
        x.derive = [] (Grids& g, const Context&) {
            auto out = g["t8"];
            for (size_t i = 0; i < out.values.size(); i++) {
                const double a = g["t8"].values[i] - g["t7"].values[i], b = g["t8"].values[i] - g["d8"].values[i];
                out.values[i] = std::isnan(a) || std::isnan(b) ? std::nanf("") : static_cast<float>((a < 6 ? 1 : a <= 10 ? 2 : 3) + (b < 6 ? 1 : b <= 12 ? 2 : 3));
            }
            g["h"] = out;
        };
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = Ramp{{{1.5, QColor{"#a9d98a"}}, {2.5, QColor{"#a9d98a"}}, {3.0, QColor{"#e3d44a"}}, {4.0, QColor{"#f0a63a"}}, {5.0, QColor{"#e0502e"}}, {6.0, QColor{"#a8206b"}}}};
        x.fillTitle = "Haines index: 2-3 very low, 4 low, 5 moderate, 6 high";
        x.legendStep = 1;
        p.push_back(x);
    }
    for (const auto& [id, label, variable] : {std::tuple{"max_updraft", "Maximum 1-hr Updraft Vertical Velocity", "MAXUVV"}, {"max_downdraft", "Maximum 1-hr Downdraft Vertical Velocity", "MAXDVV"}}) {
        auto x = make(id, label);
        const std::string name = variable;
        x.needs = [name] (int hour) { return std::vector<GfsData::Need>{{hour, {"w", name, "100-1000 mb", std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst", ""}}}; };
        x.fill = [name] (const Grids& g) { return GfsGrid::scaled(pick(g, "w"), name == "MAXDVV" ? -1.0 : 1.0); };   // the downdrafts as a positive speed
        x.ramp = lowWind();
        x.fillTitle = name == "MAXUVV" ? "Strongest updraft of the last hour (m/s, about)" : "Strongest downdraft of the last hour (m/s, about)";
        x.legendStep = 10;
        p.push_back(x);
    }
    for (const auto& [id, label] : {std::pair{"pbl_height", "PBL Height"}, {"pbl_rich_height", "PBL Height (Based on Richardson Number)"}}) {
        auto x = make(id, label);   // the model has the one boundary layer height
        x.wants = {want("h", "HPBL", "surface")};
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = ceilingRamp();
        x.ramp = Ramp{{{0, QColor{"#f4f8fb"}}, {500, QColor{"#cfe8f3"}}, {1000, QColor{"#8ccbe0"}}, {1500, QColor{"#55a868"}}, {2000, QColor{"#e3d44a"}}, {3000, QColor{"#f0a63a"}}, {4000, QColor{"#cc3d2c"}}}};
        x.fillTitle = "Boundary layer height (m)";
        x.legendStep = 500;
        p.push_back(x);
    }
    {
        auto x = make("precip_pwat", "Total Column Precipitable Water");
        x.wants = {want("pw", "PWAT", "entire atmosphere (considered as a single layer)")};
        x.fill = [] (const Grids& g) { return pick(g, "pw"); };
        x.ramp = precipitableWater();
        x.quantity = Quantity::Millimeters;
        x.fillTitle = "Precipitable water";
        x.legendStep = 0;
        p.push_back(x);
    }
    {   // transport wind (the mean wind through the mixed layer) is taken here as the mean of the winds at the heights the model gives through the lowest 320 m and the 925 and 850 mb winds when the boundary layer reaches them
        const auto levels = std::vector<std::pair<const char *, const char *>>{{"10 m above ground", "10"}, {"30 m above ground", "30"}, {"50 m above ground", "50"}, {"80 m above ground", "80"}, {"100 m above ground", "100"},
                                                                               {"160 m above ground", "160"}, {"320 m above ground", "320"}};
        const auto meanWind = [levels] (Grids& g) {
            auto u = g["u10"], v = g["v10"];
            for (size_t i = 0; i < u.values.size(); i++) {
                double su = 0.0, sv = 0.0;
                int n = 0;
                const double top = g["hp"].values[i];
                for (const auto& [level, key] : levels) {
                    (void) level;
                    const double height = std::atof(key);
                    if (n > 0 && height > top) {
                        break;
                    }
                    su += g[std::string{"u"} + key].values[i];
                    sv += g[std::string{"v"} + key].values[i];
                    n++;
                }
                for (const char * level : {"925", "850"}) {
                    const double metres = std::string{level} == "925" ? 750.0 : 1500.0;
                    if (top > metres) {
                        su += g[std::string{"u"} + level].values[i];
                        sv += g[std::string{"v"} + level].values[i];
                        n++;
                    }
                }
                u.values[i] = n ? static_cast<float>(su / n) : std::nanf("");
                v.values[i] = n ? static_cast<float>(sv / n) : std::nanf("");
            }
            g["mu"] = u;
            g["mv"] = v;
        };
        const auto windWants = [levels] {
            std::vector<GfsData::Want> w{want("hp", "HPBL", "surface"), want("u925", "UGRD", "925 mb"), want("v925", "VGRD", "925 mb"), want("u850", "UGRD", "850 mb"), want("v850", "VGRD", "850 mb")};
            for (const auto& [level, key] : levels) {
                w.push_back(want((std::string{"u"} + key).c_str(), "UGRD", level));
                w.push_back(want((std::string{"v"} + key).c_str(), "VGRD", level));
            }
            return w;
        };
        auto x = make("transport_wind", "Transport Wind and Terrain Height");
        x.wants = windWants();
        x.wants.push_back(want("z", "HGT", "surface"));
        x.derive = [meanWind] (Grids& g, const Context&) { meanWind(g); };
        x.fill = speedOf("mu", "mv");
        x.ramp = windSpeed();
        x.fillTitle = "Transport wind speed (kt)";
        x.legendStep = 10;
        ContourSet terrain;
        terrain.key = "z";
        terrain.interval = 500;
        terrain.title = "Terrain height (m)";
        terrain.color = QColor{110, 80, 40};
        terrain.width = 0.9;
        x.contours = {terrain};
        x.barbU = "mu";
        x.barbV = "mv";
        p.push_back(x);
        auto y = make("vent_rate", "Ventilation Rate");   // the boundary layer height times the transport wind: under 2350 m2/s poor, 2350-4700 fair, above good for burning
        y.wants = windWants();
        y.derive = [meanWind] (Grids& g, const Context&) {
            meanWind(g);
            auto out = g["hp"];
            for (size_t i = 0; i < out.values.size(); i++) {
                out.values[i] = static_cast<float>(g["hp"].values[i] * std::hypot(g["mu"].values[i], g["mv"].values[i]));
            }
            g["vr"] = out;
        };
        y.fill = [] (const Grids& g) { return pick(g, "vr"); };
        y.ramp = Ramp{{{0, QColor{"#b5368f"}}, {2350, QColor{"#ee7a47"}}, {4700, QColor{"#e3d44a"}}, {9000, QColor{"#7bc47f"}}, {18000, QColor{"#2b7fc0"}}}};
        y.fillTitle = "Ventilation rate (m2/s): poor under 2350, fair to 4700, then good";
        y.legendStep = 0;
        y.barbU = "mu";
        y.barbV = "mv";
        p.push_back(y);
    }
    for (const char * level : {"250", "300"}) {
        const std::string text = std::string{level} + " mb";
        auto x = make((std::string{level} + "_wnd").c_str(), (std::string{level} + "mb Wind").c_str());
        x.wants = {want("u", "UGRD", text.c_str()), want("v", "VGRD", text.c_str())};
        x.fill = speedOf("u", "v");
        x.ramp = windSpeed();
        x.fillTitle = "Wind speed (kt)";
        x.legendStep = 20;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    if (model == "RRFS" || hrrr) {   // the smoke and dust of the RRFS (its 2dfld files) and the HRRR's smoke: near the ground, in the column, where the fires and dust sources put them into the air, and the haze (section 24 of the plan)
        const auto aerosol = [] (const char * kind, const char * size) { return std::string{"aerosol="} + kind + "|aerosol_size " + size; };
        const auto clean = [] (Ramp r) {   // the air quality index's colors from a third of "good" up: clean air is left clear, the green is where there is something
            r.stops.front().first = r.stops[1].first / 3.0;
            return r;
        };
        const Ramp smokeColumn{{{0, QColor{217, 205, 184, 0}}, {2, QColor{217, 205, 184, 0}}, {5, QColor{"#e3d7c2"}}, {10, QColor{"#c9b79a"}}, {25, QColor{"#a38a6c"}}, {50, QColor{"#7a604a"}},
                                {100, QColor{"#523d30"}}, {250, QColor{"#2b1f19"}}, {500, QColor{"#120b08"}}}};   // milligrams a square meter
        const Ramp dust{{{0, QColor{246, 226, 150, 0}}, {5, QColor{246, 226, 150, 0}}, {10, QColor{"#f5dc8a"}}, {25, QColor{"#eab552"}}, {50, QColor{"#d98a3a"}}, {100, QColor{"#c0602a"}},
                         {250, QColor{"#9a3a25"}}, {500, QColor{"#6f1d3a"}}, {1000, QColor{"#4a0f5c"}}}};
        const Ramp flux{{{0, QColor{255, 230, 120, 0}}, {0.05, QColor{255, 230, 120, 0}}, {0.1, QColor{"#ffe678"}}, {0.5, QColor{"#ffb43c"}}, {2, QColor{"#ff6e1e"}}, {10, QColor{"#dc1e14"}},
                         {50, QColor{"#8c0a3c"}}, {250, QColor{"#3c0050"}}}};   // micrograms a square meter a second: the sources (a fire's flux runs to thousands)
        const Ramp optical{{{0, QColor{232, 243, 216, 0}}, {0.05, QColor{232, 243, 216, 0}}, {0.1, QColor{"#e8f3d8"}}, {0.25, QColor{"#cfe6a0"}}, {0.5, QColor{"#f1e27a"}}, {1.0, QColor{"#f0a63a"}},
                            {2.0, QColor{"#d63a2a"}}, {3.0, QColor{"#7a1a6a"}}}};
        struct Spec {
            const char * id;
            const char * label;
            const char * variable;
            const char * level;
            std::string detail;
            double scale;
            Ramp ramp;
            const char * title;
            int spread{0};   // a source is one 3 km cell, a speck on a map of the country: drawn over this many cells round it (the largest value near each cell; the values are the sources' own)
            bool fires{false};   // the fires (the smoke's emission flux) painted over the smoke, so the plume is seen with where it comes from
        };
        const char * surface = "8 m above ground";
        const char * column = "entire atmosphere (considered as a single layer)";
        const std::vector<Spec> rrfsSpecs{
            {"pm25_sfc", "Fine particles (PM2.5) near the surface", "MASSDEN", surface, aerosol("Total aerosol", "<2.5e-06"), 1e9, clean(airQuality({9, 35.4, 55.4, 125.4, 225.4})),
             "PM2.5 at 8 m, the hour's mean: the RRFS's smoke and dust (no traffic or industry; micrograms per cubic meter, US air quality index colors)"},
            {"pm10_aq", "Coarse particles (PM10) near the surface", "MASSDEN", surface, aerosol("Total aerosol", "<1e-05"), 1e9, clean(airQuality({54, 154, 254, 354, 424})),
             "PM10 at 8 m, the hour's mean: the RRFS's smoke and dust (no traffic or industry; micrograms per cubic meter, US air quality index colors)"},
            {"smoke_sfc", "Smoke near the surface", "MASSDEN", surface, aerosol("Particulate organic matter dry", "<2.5e-06"), 1e9, clean(airQuality({9, 35.4, 55.4, 125.4, 225.4})),
             "Smoke (organic matter, PM2.5) at 8 m (micrograms per cubic meter); the fires in reds", 0, true},
            {"dust_sfc", "Dust near the surface", "MASSDEN", surface, aerosol("Dust dry", "<2.5e-06"), 1e9, dust, "Fine dust (PM2.5) at 8 m (micrograms per cubic meter)"},
            {"smoke_column", "Smoke aloft (column)", "COLMD", column, aerosol("Particulate organic matter dry", "<2.5e-06"), 1e6, smokeColumn,
             "Smoke in the whole column (milligrams per square meter): the haze aloft, even where the ground air is clean; the fires in reds", 0, true},
            {"dust_column", "Dust aloft (column)", "COLMD", column, aerosol("Dust dry", "<1e-05"), 1e6, dust, "Dust (PM10) in the whole column (milligrams per square meter)"},
            {"smoke_emission", "Fire smoke emissions", "AEMFLX", column, aerosol("Particulate organic matter dry", "<2.5e-06"), 1e9, flux,
             "Smoke put into the air (micrograms per square meter per second): the fires the model knows of (each drawn about 10 km across)", 3},
            {"dust_emission", "Dust emissions", "AEMFLX", column, aerosol("Dust dry", "<1e-05"), 1e9, flux, "Dust lifted into the air (micrograms per square meter per second): the dust sources (each drawn about 10 km across)", 3},
            {"aod", "Aerosol optical thickness", "AOTK", column, "", 1.0, optical, "Aerosol optical thickness (how much the haze dims the sun; over 1 is thick)"},
        };
        // the HRRR (and HRRR-AK) has its smoke model's fields only: the smoke near the ground and in the column, and the optical thickness; no dust, no emission flux (checked 16 Nov 2026)
        const std::vector<Spec> hrrrSpecs{
            {"smoke_sfc", "Smoke near the surface", "MASSDEN", surface, "", 1e9, clean(airQuality({9, 35.4, 55.4, 125.4, 225.4})), "Smoke (PM2.5) at 8 m, the HRRR-Smoke model's (micrograms per cubic meter)"},
            {"smoke_column", "Smoke aloft (column)", "COLMD", column, "", 1e6, smokeColumn, "Smoke in the whole column (milligrams per square meter): the haze aloft, even where the ground air is clean"},
            {"aod", "Aerosol optical thickness", "AOTK", column, "", 1.0, optical, "Aerosol optical thickness (how much the haze dims the sun; over 1 is thick)"},
        };
        const auto& specs = hrrr ? hrrrSpecs : rrfsSpecs;
        for (const auto& spec : specs) {
            auto x = make(spec.id, spec.label);
            x.wants.push_back(GfsData::Want{"a", spec.variable, spec.level, "", spec.detail});
            if (spec.fires) {
                x.wants.push_back(GfsData::Want{"e", "AEMFLX", column, "", aerosol("Particulate organic matter dry", "<2.5e-06")});
                x.overlays.push_back({"fires", flux});
            }
            wind10(x);
            const double factor = spec.scale;
            const int spread = spec.spread;
            const bool fires = spec.fires;
            const auto widen = [] (GfsGrid::Grid out, int spread) {
                const auto source = out;
                for (int r = 0; r < out.rows; r++) {
                    for (int c = 0; c < out.columns; c++) {
                        float best = std::numeric_limits<float>::quiet_NaN();
                        for (int dr = -spread; dr <= spread; dr++) {
                            for (int dc = -spread; dc <= spread; dc++) {
                                const int rr = r + dr, cc = c + dc;
                                if (rr >= 0 && rr < out.rows && cc >= 0 && cc < out.columns && dr * dr + dc * dc <= spread * spread) {
                                    const float v = source.at(cc, rr);
                                    if (!std::isnan(v) && (std::isnan(best) || v > best)) {
                                        best = v;
                                    }
                                }
                            }
                        }
                        out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = best;
                    }
                }
                return out;
            };
            x.derive = [factor, spread, fires, widen] (Grids& g, const Context&) {
                auto out = GfsGrid::scaled(pick(g, "a"), factor);
                if (spread > 0 && !out.empty()) {
                    out = widen(out, spread);
                }
                g["fill"] = std::move(out);
                if (fires && g.count("e") && !g["e"].empty()) {   // a fire is a flux of at least 0.1 microgram a square meter a second; elsewhere clear, so the smoke shows
                    auto fire = widen(GfsGrid::scaled(g["e"], 1e9), 3);
                    for (auto& v : fire.values) {
                        v = v >= 0.1f ? v : std::numeric_limits<float>::quiet_NaN();
                    }
                    g["fires"] = std::move(fire);
                }
            };
            x.fill = [] (const Grids& g) { return pick(g, "fill"); };
            x.ramp = spec.ramp;
            x.fillTitle = spec.title;
            x.legendStep = 0;
            p.push_back(x);
        }
    }
}

}  // namespace detail
}  // namespace GfsChart
