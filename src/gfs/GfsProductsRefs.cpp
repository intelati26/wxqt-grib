// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The RRFS ensemble (REFS): the members' chances, paintballs and the mean and spread.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void refsProducts(std::vector<Product>& p) {
    const auto make = [] (const char * id, const char * label) {
        Product x;
        x.source = "REFS";
        x.id = id;
        x.label = label;
        return x;
    };
    const auto sea = [] (int m) { return GfsData::Want{"p" + std::to_string(m), "MSLET", "mean sea level", "", "", "m00" + std::to_string(m)}; };
    (void) sea;
    // the same field from each member: keys "<prefix>1" ... "<prefix>5"
    const auto members = [] (const char * prefix, const char * variable, const char * level, const std::string& forecast) {
        std::vector<GfsData::Want> out;
        for (int m = 1; m <= 5; m++) {
            out.push_back({std::string{prefix} + std::to_string(m), variable, level, forecast, "", "m00" + std::to_string(m)});
        }
        return out;
    };
    const auto hourly = [] (int hour) { return std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst"; };
    // the fraction of the members at or above a threshold, averaged over the neighborhood
    const auto fraction = [] (const Grids& g, const char * prefix, double threshold) {
        auto out = pick(g, (std::string{prefix} + "1").c_str());
        for (size_t i = 0; i < out.values.size(); i++) {
            int n = 0, over = 0;
            for (int m = 1; m <= 5; m++) {
                const float v = g.at(std::string{prefix} + std::to_string(m)).values[i];
                if (!std::isnan(v)) {
                    n++;
                    over += v >= threshold ? 1 : 0;
                }
            }
            out.values[i] = n > 0 ? static_cast<float>(100.0 * over / n) : std::nanf("");
        }
        return GfsGrid::smoothed(out, 6);
    };
    const auto reflectivityOf = [&] (const char * id, const char * label, double threshold) {
        auto x = make(id, label);
        x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
        x.fill = [fraction, threshold] (const Grids& g) { return fraction(g, "r", threshold); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance of the echo reaching the level (% of the members, nearby)";
        x.legendStep = 10;
        return x;
    };
    p.push_back(reflectivityOf("prob_refc_40", "Probability of Composite Reflectivity of 40 dBZ or more", 40.0));
    p.push_back(reflectivityOf("prob_refc_50", "Probability of Composite Reflectivity of 50 dBZ or more", 50.0));
    {
        auto x = make("prob_uphl_75", "Probability of 2-5 km Updraft Helicity of 75 or more (last hour)");
        x.needs = [members, hourly] (int hour) {
            std::vector<GfsData::Need> out;
            for (auto w : members("h", "MXUPHL", "5000-2000 m above ground", hourly(hour))) {
                out.push_back({hour, w});
            }
            return out;
        };
        x.fill = [fraction] (const Grids& g) { return fraction(g, "h", 75.0); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance of rotating updrafts (% of the members, nearby)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("max_uphl", "Strongest 2-5 km Updraft Helicity of any member (last hour)");
        x.needs = [members, hourly] (int hour) {
            std::vector<GfsData::Need> out;
            for (auto w : members("h", "MXUPHL", "5000-2000 m above ground", hourly(hour))) {
                out.push_back({hour, w});
            }
            return out;
        };
        x.fill = [] (const Grids& g) {
            auto out = pick(g, "h1");
            for (size_t i = 0; i < out.values.size(); i++) {
                float best = out.values[i];
                for (int m = 2; m <= 5; m++) {
                    const float v = g.at("h" + std::to_string(m)).values[i];
                    best = std::isnan(best) || v > best ? v : best;
                }
                out.values[i] = best;
            }
            return out;
        };
        x.ramp = updraftHelicity();
        x.palette = "uh";
        x.fillTitle = "Updraft helicity (m2/s2)";
        x.legendStep = 0;
        p.push_back(x);
    }
    // the SPC's neighborhood chance for the paintballs: the grids "chance" (%) and an all-clear "base" for the fill
    const auto paintballChance = [] (Grids& g, const Context&) {
            constexpr int radius = 13;
            GfsGrid::Grid out;
            for (const auto& [key, grid] : g) {   // (any layer left: the first one is the pattern of the grid)
                if (key[0] == 'r' && !grid.empty()) {
                    out = grid;
                    break;
                }
            }
            if (out.empty()) {
                return;
            }
            const int columns = out.columns, rows = out.rows;
            std::vector<int> count(out.values.size(), 0), have(out.values.size(), 0);
            for (const auto& [key, grid] : g) {
                if (key[0] != 'r' || grid.empty() || grid.values.size() != out.values.size()) {
                    continue;
                }
                // a round neighborhood: each row counts the hits of the rows within the radius, over the width of the circle there (a running total along each row makes that one subtraction)
                std::vector<int> along(static_cast<size_t>(rows) * static_cast<size_t>(columns + 1), 0);
                for (int r = 0; r < rows; r++) {
                    for (int c = 0; c < columns; c++) {
                        const float v = grid.values[static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(c)];
                        along[static_cast<size_t>(r) * static_cast<size_t>(columns + 1) + static_cast<size_t>(c) + 1] = along[static_cast<size_t>(r) * static_cast<size_t>(columns + 1) + static_cast<size_t>(c)] + (!std::isnan(v) && v >= 40.0f ? 1 : 0);
                    }
                }
                int half[2 * radius + 1];
                for (int d = -radius; d <= radius; d++) {
                    half[d + radius] = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - d * d)) + 0.5);
                }
                for (int r = 0; r < rows; r++) {
                    for (int c = 0; c < columns; c++) {
                        const size_t at = static_cast<size_t>(r) * static_cast<size_t>(columns) + static_cast<size_t>(c);
                        have[at] += std::isnan(grid.values[at]) ? 0 : 1;
                        bool near = false;
                        for (int d = -radius; d <= radius && !near; d++) {
                            const int rr = r + d;
                            if (rr < 0 || rr >= rows) {
                                continue;
                            }
                            const int lo = std::max(0, c - half[d + radius]), hi = std::min(columns - 1, c + half[d + radius]);
                            const size_t row = static_cast<size_t>(rr) * static_cast<size_t>(columns + 1);
                            near = along[row + static_cast<size_t>(hi) + 1] - along[row + static_cast<size_t>(lo)] > 0;
                        }
                        count[at] += near ? 1 : 0;
                    }
                }
            }
            for (size_t i = 0; i < out.values.size(); i++) {
                out.values[i] = have[i] > 0 ? static_cast<float>(100.0 * count[i] / have[i]) : std::nanf("");
            }
            g["chance"] = GfsGrid::smoothed(out, 20);
            auto none = out;   // the fill is clear (a layer taken out cannot leave the chart without one): every layer is an overlay in its own color
            std::fill(none.values.begin(), none.values.end(), std::nanf(""));
            g["base"] = std::move(none);
        };
    {   // each member's 40 dBZ echoes in a color of its own, so where they agree and where they differ shows
        auto x = make("refc_paintball", "Paintball: Composite Reflectivity of 40 dBZ or more, each member");
        x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
        const char * names[5] = {"#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e"};
        const auto above = [] (const char * color) {
            QColor solid{color};
            solid.setAlpha(176);
            return Ramp{{{0, QColor{255, 255, 255, 0}}, {39.99, QColor{255, 255, 255, 0}}, {40.0, solid}, {100, solid}}};
        };
        x.fill = [] (const Grids& g) { return pick(g, "base"); };
        x.ramp = above(names[0]);
        const char * layerNames[5] = {"REFS member 1 (control)", "REFS member 2", "REFS member 3", "REFS member 4", "REFS member 5"};
        for (int m = 1; m <= 5; m++) {
            x.layers.push_back({"r" + std::to_string(m), layerNames[m - 1]});
            x.overlays.push_back({"r" + std::to_string(m), above(names[m - 1])});
        }
        x.derive = paintballChance;
        ContourSet c;
        c.key = "chance";
        c.interval = 20.0;
        c.base = 10.0;
        c.minimum = 9.0;
        c.title = "40 dBZ chance (%)";
        c.color = QColor{20, 20, 20};
        c.uniform = true;
        c.width = 1.4;
        x.contours.push_back(c);
        x.fillTitle = "Members: 1 blue, 2 red, 3 green, 4 purple, 5 orange (40 dBZ or more)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {   // the same, with NSSL's experimental MPAS as a sixth (pink) and seventh (teal) set of echoes among the five members: where it sits among them, inside or outside what the ensemble spreads over
        auto x = make("refc_paintball_mpas", "Paintball: Composite Reflectivity of 40 dBZ or more, each member and the NSSL MPAS runs");
        x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
        x.extras = {{"MPAS-HT", GfsData::Want{"r6", "REFC", "entire atmosphere", "", "", ""}, "MPAS-HT"}, {"MPAS-RN", GfsData::Want{"r7", "REFC", "entire atmosphere", "", "", ""}, "MPAS-RN"}};
        const char * names[7] = {"#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e", "#e377c2", "#17becf"};
        const auto above = [] (const char * color) {
            QColor solid{color};
            solid.setAlpha(176);
            return Ramp{{{0, QColor{255, 255, 255, 0}}, {39.99, QColor{255, 255, 255, 0}}, {40.0, solid}, {100, solid}}};
        };
        x.fill = [] (const Grids& g) { return pick(g, "base"); };
        x.ramp = above(names[0]);
        const char * layerNames[7] = {"REFS member 1 (control)", "REFS member 2", "REFS member 3", "REFS member 4", "REFS member 5", "MPAS-HT (NSSL)", "MPAS-RN (NSSL)"};
        for (int m = 1; m <= 7; m++) {
            x.layers.push_back({"r" + std::to_string(m), layerNames[m - 1]});
            x.overlays.push_back({"r" + std::to_string(m), above(names[m - 1])});
        }
        x.derive = paintballChance;
        ContourSet c;
        c.key = "chance";
        c.interval = 20.0;
        c.base = 10.0;
        c.minimum = 9.0;
        c.title = "40 dBZ chance (%)";
        c.color = QColor{20, 20, 20};
        c.uniform = true;
        c.width = 1.4;
        x.contours.push_back(c);
        x.fillTitle = "Members: 1 blue, 2 red, 3 green, 4 purple, 5 orange; NSSL MPAS-HT pink, MPAS-RN teal (40 dBZ or more; experimental, not for forecasting)";
        x.legendStep = 0;
        p.push_back(x);
    }
    {   // the REFS members with NSSL's two MPAS runs as one pool: the share of them that reach the level. "Most recent" is the newest run of each (up to 7 members); the lagged pools add the
        // runs one and two cycles older of each (up to 14 and 21 members), which is the point: the earlier runs are what the slow MPAS runs have to give. A member that cannot be read is left out
        struct Lagged {
            int lag;
            const char * id;
            const char * label;
        };
        for (const auto& level : {std::pair{40.0, "40"}, std::pair{50.0, "50"}}) {
            for (const auto& lagged : {Lagged{0, "", "most recent runs"}, Lagged{1, "_lag2", "last 2 runs of each"}, Lagged{2, "_lag3", "last 3 runs of each"}}) {
                auto x = make((std::string{"pool_refc_"} + level.second + lagged.id).c_str(), (std::string{"Pooled REFS + MPAS: chance of "} + level.second + " dBZ or more, " + lagged.label).c_str());
                x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
                for (int lag = 0; lag <= lagged.lag; lag++) {
                    const std::string suffix = lag == 0 ? std::string{} : "_" + std::to_string(lag);
                    x.extras.push_back({"MPAS-HT", GfsData::Want{"rh" + suffix, "REFC", "entire atmosphere", "", "", ""}, lag == 0 ? "MPAS-HT" : "", lag});
                    x.extras.push_back({"MPAS-RN", GfsData::Want{"rn" + suffix, "REFC", "entire atmosphere", "", "", ""}, lag == 0 ? "MPAS-RN" : "", lag});
                    if (lag > 0) {
                        for (int m = 1; m <= 5; m++) {
                            x.extras.push_back({"REFS", GfsData::Want{"r" + std::to_string(m) + suffix, "REFC", "entire atmosphere (considered as a single layer)", "", "", "m00" + std::to_string(m)}, "", lag});
                        }
                    }
                }
                const double threshold = level.first;
                x.fill = [threshold] (const Grids& g) {   // every REFC grid there is, the members that were read: the share of them at or above the level
                    auto out = pick(g, "r1");
                    for (size_t i = 0; i < out.values.size(); i++) {
                        int n = 0, over = 0;
                        for (const auto& [key, grid] : g) {
                            if (key[0] == 'r' && !grid.empty() && i < grid.values.size() && !std::isnan(grid.values[i])) {
                                n++;
                                over += grid.values[i] >= threshold ? 1 : 0;
                            }
                        }
                        out.values[i] = n > 0 ? static_cast<float>(100.0 * over / n) : std::nanf("");
                    }
                    return GfsGrid::smoothed(out, 6);
                };
                x.ramp = probability();
                x.palette = "prob";
                x.fillTitle = std::string{"Share of the pooled members (REFS, NSSL MPAS-HT and MPAS-RN; "} + lagged.label + ") at the level, nearby (experimental MPAS, not for forecasting)";
                x.legendStep = 10;
                p.push_back(x);
            }
        }
    }
    {   // the paintball as the SPC's REFS viewer draws it, with NSSL's two MPAS runs added: the five REFS members, the HRRR and the HRRR of 6 and 12 hours before (the same grays) and the MPAS-HT and
        // MPAS-RN, with the lines of the chance of 40 dBZ (the share of the members over it, nearby: 10, 30, 50, 70 and 90 per cent). A run that cannot be read is left out of the paint and the count
        auto x = make("refc_paintball_spc", "Paintball: 40 dBZ or more, REFS, HRRR and NSSL MPAS, with the chance");
        x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
        x.extras = {{"HRRR", GfsData::Want{"r6", "REFC", "entire atmosphere", "", "", ""}, "HRRR", 0}, {"HRRR", GfsData::Want{"r7", "REFC", "entire atmosphere", "", "", ""}, "", 6},
                    {"HRRR", GfsData::Want{"r8", "REFC", "entire atmosphere", "", "", ""}, "", 12}, {"MPAS-HT", GfsData::Want{"r9", "REFC", "entire atmosphere", "", "", ""}, "MPAS-HT", 0},
                    {"MPAS-RN", GfsData::Want{"r10", "REFC", "entire atmosphere", "", "", ""}, "MPAS-RN", 0},
                    {"REFS", GfsData::Want{"r11", "REFC", "entire atmosphere (considered as a single layer)", "", "", "m001"}, "", 1},   // the control (member 1) of the REFS runs 6 and 12 hours older
                    {"REFS", GfsData::Want{"r12", "REFC", "entire atmosphere (considered as a single layer)", "", "", "m001"}, "", 2}};
        const char * names[12] = {"#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e", "#4c4c4c", "#7f7f7f", "#b3b3b3", "#e377c2", "#17becf", "#4cd3ff", "#b3ecff"};
        const auto above = [] (const char * color) {
            QColor solid{color};
            solid.setAlpha(176);
            return Ramp{{{0, QColor{255, 255, 255, 0}}, {39.99, QColor{255, 255, 255, 0}}, {40.0, solid}, {100, solid}}};
        };
        x.derive = paintballChance;
        x.fill = [] (const Grids& g) { return pick(g, "base"); };
        x.ramp = above(names[0]);
        const char * layerNames[12] = {"REFS member 1 (control)", "REFS member 2", "REFS member 3", "REFS member 4", "REFS member 5", "HRRR", "HRRR -6h", "HRRR -12h", "MPAS-HT (NSSL)", "MPAS-RN (NSSL)", "REFS control -6h", "REFS control -12h"};
        for (int m = 1; m <= 12; m++) {
            x.layers.push_back({"r" + std::to_string(m), layerNames[m - 1]});
        }
        for (int m = 1; m <= 12; m++) {
            x.overlays.push_back({"r" + std::to_string(m), above(names[m - 1])});
        }
        ContourSet c;
        c.key = "chance";
        c.interval = 20.0;
        c.base = 10.0;
        c.minimum = 9.0;
        c.title = "40 dBZ chance (%)";
        c.color = QColor{20, 20, 20};
        c.uniform = true;
        c.width = 1.4;
        x.contours.push_back(c);
        x.fillTitle = "REFS 1-5; HRRR grays (now, -6, -12); MPAS-HT pink, MPAS-RN teal; REFS member 1 -6 and -12 light blue (40 dBZ or more)";
        x.legendStep = 0;
        p.push_back(x);
    }
    // one member of the field on its own (the old REFS screen's "RRFS Ensemble Member" rows): member 1 here, the screen's member picker swaps it for 2 to 5 (withMember)
    {
        const auto one = [&] (const char * key, const char * label, const char * variable, const char * level, bool hourlyMax, const Ramp& ramp, const char * title, int legendStep,
                              Quantity quantity, float scale) {
            auto x = make((std::string{"member_"} + key).c_str(), (std::string{"Member: "} + label).c_str());
            const GfsData::Want want{"v", variable, level, "", "", "m001"};
            if (!hourlyMax) {
                x.wants = {want};
            } else {
                x.needs = [want] (int hour) {
                    auto w = want;
                    w.forecast = std::to_string(std::max(hour - 1, 0)) + "-" + std::to_string(hour) + " hour max fcst";
                    return std::vector<GfsData::Need>{{hour, w}};
                };
            }
            x.fill = [scale] (const Grids& g) {
                auto out = pick(g, "v");
                if (scale != 1.0f) {
                    for (auto& v : out.values) {
                        v *= scale;
                    }
                }
                return out;
            };
            x.ramp = ramp;
            if (hourlyMax) {
                x.palette = "uh";
            }
            x.quantity = quantity;
            x.fillTitle = title;
            x.legendStep = legendStep;
            p.push_back(x);
        };
        one("refc", "Composite Reflectivity", "REFC", "entire atmosphere (considered as a single layer)", false, reflectivity(), "Composite reflectivity (dBZ)", 10, Quantity::Other, 1.0f);
        one("tmp2m", "2 m Temperature", "TMP", "2 m above ground", false, temperature(), "2 m temperature", 5, Quantity::Temperature, 1.0f);
        one("cape", "Surface CAPE", "CAPE", "surface", false, capeRamp(), "Surface CAPE (J/kg)", 500, Quantity::Other, 1.0f);
        one("gust", "Surface Wind Gust", "GUST", "surface", false, windSpeed(), "Surface wind gust (kt)", 20, Quantity::Other, 1.943844f);
        one("uphl", "2-5 km Updraft Helicity (last hour)", "MXUPHL", "5000-2000 m above ground", true, updraftHelicity(), "Updraft helicity (m2/s2)", 0, Quantity::Other, 1.0f);
    }
    for (const auto& [id, label, spread] : {std::tuple{"mean_2m_temp", "Ensemble Mean 2 m Temperature", false}, {"spread_2m_temp", "2 m Temperature Spread among the members", true}}) {
        auto x = make(id, label);
        x.wants = members("t", "TMP", "2 m above ground", "");
        x.fill = [spread] (const Grids& g) {
            auto out = pick(g, "t1");
            for (size_t i = 0; i < out.values.size(); i++) {
                double sum = 0.0, square = 0.0;
                int n = 0;
                for (int m = 1; m <= 5; m++) {
                    const float v = g.at("t" + std::to_string(m)).values[i];
                    if (!std::isnan(v)) {
                        sum += v;
                        square += static_cast<double>(v) * v;
                        n++;
                    }
                }
                out.values[i] = n == 0 ? std::nanf("") : spread ? static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n)))) : static_cast<float>(sum / n);
            }
            return out;
        };
        x.ramp = spread ? spreadRamp(5.0) : temperature();
        x.quantity = spread ? Quantity::Other : Quantity::Temperature;
        x.fillTitle = spread ? "Spread of the 2 m temperature (C)" : "Mean 2 m temperature";
        x.legendStep = spread ? 1 : 5;
        p.push_back(x);
    }

    // the combinations: the deterministic RRFS in color, and what the members say about it drawn over it as lines (the model's own run is one thing, the ensemble's chance another)
    const auto rrfsWant = [] (const char * key, const char * variable, const char * level, const std::string& forecast) {
        return GfsData::Want{key, variable, level, forecast, "", "rrfs"};
    };
    const auto withChance = [] (const char * key, const char * title, QColor color) {
        ContourSet c;
        c.key = key;
        c.base = 30.0;
        c.minimum = 30.0;
        c.interval = 30.0;   // the 30, 60 and 90 per cent lines
        c.title = title;
        c.color = color;
        c.width = 2.2;
        return c;
    };
    {
        auto x = make("combo_refc_chance", "RRFS Radar with the Members' Chance of 40 dBZ or more");
        x.wants = members("r", "REFC", "entire atmosphere (considered as a single layer)", "");
        x.wants.push_back(rrfsWant("d", "REFC", "entire atmosphere (considered as a single layer)", ""));
        x.derive = [fraction] (Grids& g, const Context&) { g["chance"] = fraction(g, "r", 40.0); };
        x.fill = [] (const Grids& g) { return pick(g, "d"); };
        x.ramp = reflectivity();
        x.fillTitle = "RRFS composite reflectivity (dBZ)";
        x.legendStep = 10;
        x.contours = {withChance("chance", "Chance of 40 dBZ or more in the 5 members (%)", QColor{20, 20, 20})};
        p.push_back(x);
    }
    {
        auto x = make("combo_uphl_chance", "RRFS Updraft Helicity with the Members' Chance of 75 or more");
        x.needs = [members, rrfsWant, hourly] (int hour) {
            std::vector<GfsData::Need> out;
            for (auto w : members("h", "MXUPHL", "5000-2000 m above ground", hourly(hour))) {
                out.push_back({hour, w});
            }
            out.push_back({hour, rrfsWant("d", "MXUPHL", "5000-2000 m above ground", hourly(hour))});
            return out;
        };
        x.derive = [fraction] (Grids& g, const Context&) { g["chance"] = fraction(g, "h", 75.0); };
        x.fill = [] (const Grids& g) { return pick(g, "d"); };
        x.ramp = updraftHelicity();
        x.palette = "uh";
        x.fillTitle = "RRFS 2-5 km updraft helicity in the last hour (m2/s2)";
        x.legendStep = 0;
        x.contours = {withChance("chance", "Chance of 75 or more in the 5 members (%)", QColor{20, 20, 20})};
        p.push_back(x);
    }
    {
        auto x = make("combo_temp_spread", "RRFS 2 m Temperature with the Members' Spread");
        x.wants = members("t", "TMP", "2 m above ground", "");
        x.wants.push_back(rrfsWant("d", "TMP", "2 m above ground", ""));
        x.derive = [] (Grids& g, const Context&) {
            auto out = g["t1"];
            for (size_t i = 0; i < out.values.size(); i++) {
                double sum = 0.0, square = 0.0;
                int n = 0;
                for (int m = 1; m <= 5; m++) {
                    const float v = g.at("t" + std::to_string(m)).values[i];
                    if (!std::isnan(v)) {
                        sum += v;
                        square += static_cast<double>(v) * v;
                        n++;
                    }
                }
                out.values[i] = n == 0 ? std::nanf("") : static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n))));
            }
            g["spread"] = GfsGrid::smoothed(out, 2);
        };
        x.fill = [] (const Grids& g) { return pick(g, "d"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "RRFS 2 m temperature";
        x.legendStep = 5;
        ContourSet spread;
        spread.key = "spread";
        spread.base = 1.0;
        spread.minimum = 1.0;
        spread.interval = 1.0;
        spread.title = "Spread of the members (C)";
        spread.color = QColor{20, 20, 20};
        spread.width = 1.6;
        x.contours = {spread};
        p.push_back(x);
    }
    // the ready-made products: the window is (hour - period) to the hour, as the file writes it
    const auto window = [] (int hour, int period) { return std::to_string(std::max(hour - period, 0)) + "-" + std::to_string(hour) + " hour acc fcst"; };
    for (const auto period : {1, 3}) {
        auto x = make(period == 1 ? "mean_precip_p01" : "mean_precip_p03", period == 1 ? "Ensemble Mean Precipitation, 1 hour" : "Ensemble Mean Precipitation, 3 hours");
        x.needs = [window, period] (int hour) { return std::vector<GfsData::Need>{{hour, {"a", "APCP", "surface", window(hour, period), "wt ens mean", "ens:avrg"}}}; };
        x.fill = [] (const Grids& g) { return pick(g, "a"); };
        x.ramp = precipitation();
    x.palette = "precip";
        x.quantity = Quantity::Millimeters;
        x.fillTitle = period == 1 ? "Mean precipitation in the last hour" : "Mean precipitation in the last 3 hours";
        x.legendStep = 0;
        p.push_back(x);
    }
    struct Prob {
        const char * id;
        const char * variable;
        int period;
        const char * detail;
        const char * title;
    };
    for (const auto& q : {Prob{"prob_precip_1h_0.25in", "APCP", 1, "prob >6.35", "Probability of 0.25 in or more of rain in an hour"}, Prob{"prob_precip_1h_0.5in", "APCP", 1, "prob >12.7", "Probability of 0.5 in or more of rain in an hour"},
                          Prob{"prob_precip_1h_1in", "APCP", 1, "prob >25.4", "Probability of 1 in or more of rain in an hour"}, Prob{"prob_precip_3h_0.5in", "APCP", 3, "prob >12.7", "Probability of 0.5 in or more of rain in 3 hours"},
                          Prob{"prob_precip_3h_1in", "APCP", 3, "prob >25.4", "Probability of 1 in or more of rain in 3 hours"}, Prob{"prob_precip_3h_2in", "APCP", 3, "prob >50.8", "Probability of 2 in or more of rain in 3 hours"},
                          Prob{"prob_snow_3h_1in", "ASNOW", 3, "prob >0.025", "Probability of 1 in or more of snow in 3 hours"}, Prob{"prob_snow_3h_3in", "ASNOW", 3, "prob >0.076", "Probability of 3 in or more of snow in 3 hours"}}) {
        auto x = make(q.id, q.title);
        const auto variable = std::string{q.variable};
        const auto detail = std::string{q.detail};
        const int period = q.period;
        x.needs = [window, variable, detail, period] (int hour) { return std::vector<GfsData::Need>{{hour, {"a", variable, "surface", window(hour, period), detail, "ens:eas"}}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "a"), 100.0); };   // a fraction -> percent
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance (%, within the neighborhood)";
        x.legendStep = 10;
        p.push_back(x);
    }
    // ---- the maps SREF had (REFS replaces it): the ensemble mean, spread and probability files of REFS itself (kind "mean", "sprd", "prob", "eas": the 5 members, on the 3 km grid) and,
    // where those hold no such field, the mean of the members' own fields. Five members: the chances come in steps of 20 per cent (the neighborhood ones, "eas", are smoother).
    const auto ens = [] (const std::string& key, const char * variable, const char * level, const std::string& forecast, const char * kind, const char * detail) {
        return GfsData::Want{key, variable, level, forecast, detail, std::string{"ens:"} + kind};
    };
    const auto atHour = [] (int hour) { return std::to_string(hour) + " hour fcst"; };
    const auto accum = [] (int from, int to) { return std::to_string(from) + "-" + std::to_string(to) + " hour acc fcst"; };
    const auto meanOfMembers = [] (Grids& g, const char * prefix, const char * out) {
        auto mean = g.at(std::string{prefix} + "1");
        for (size_t i = 0; i < mean.values.size(); i++) {
            double sum = 0.0;
            int n = 0;
            for (int m = 1; m <= 5; m++) {
                const float v = g.at(std::string{prefix} + std::to_string(m)).values[i];
                if (!std::isnan(v)) {
                    sum += v;
                    n++;
                }
            }
            mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
        }
        g[out] = std::move(mean);
    };
    for (const int period : {6, 12, 24}) {   // the mean precipitation of 6, 12 and 24 hours: the mean's own 6 hour amounts added up
        auto x = make(("ens_precip_p" + std::string{period < 10 ? "0" : ""} + std::to_string(period)).c_str(), ("Mean " + std::to_string(period) + "-hour Precipitation").c_str());
        x.needs = [ens, accum, period] (int hour) {
            std::vector<GfsData::Need> out;
            if (hour < period) {
                return out;
            }
            int n = 0;
            for (int end = hour; end > hour - period; end -= 6) {
                out.push_back({end, ens("a" + std::to_string(n++), "APCP", "surface", accum(end - 6, end), "mean", "wt ens mean")});
            }
            return out;
        };
        x.derive = [] (Grids& g, const Context&) {
            auto sum = g["a0"];
            for (int k = 1; g.count("a" + std::to_string(k)); k++) {
                for (size_t i = 0; i < sum.values.size(); i++) {
                    sum.values[i] += g["a" + std::to_string(k)].values[i];
                }
            }
            g["f"] = std::move(sum);
        };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = precipitation();
        x.palette = "precip";
        x.quantity = Quantity::Millimeters;
        x.fillTitle = "Mean precipitation in the last " + std::to_string(period) + " hours";
        x.legendStep = 0;
        p.push_back(x);
    }
    for (const auto& [id, inches, millimeters] : {std::tuple{"ens_prob_precip_6h_0.25in", "0.25", "prob >6.35"}, {"ens_prob_precip_6h_0.5in", "0.5", "prob >12.7"}, {"ens_prob_precip_6h_1in", "1", "prob >25.4"}}) {
        auto x = make(id, (std::string{"Probability of 6-hour Precipitation over "} + inches + " in").c_str());
        const std::string detail = millimeters;
        x.needs = [ens, accum, detail] (int hour) { return hour < 6 ? std::vector<GfsData::Need>{} : std::vector<GfsData::Need>{{hour, ens("a", "APCP", "surface", accum(hour - 6, hour), "eas", detail.c_str())}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "a"), 100.0); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = std::string{"Chance of more than "} + inches + " in of rain in 6 hours (%, within the neighborhood)";
        x.legendStep = 10;
        p.push_back(x);
    }
    for (const bool spread : {false, true}) {   // snow since the start of the run: the mean and the spread of the members' own totals (the mean file has the total only for the first 12 hours)
        auto x = make(spread ? "ens_snow_total_sprd" : "ens_snow_total_mean", spread ? "Snow Total Spread" : "Snow Total Mean");
        x.needs = [members, accum] (int hour) {
            std::vector<GfsData::Need> out;
            if (hour < 1) {
                return out;
            }
            for (auto w : members("s", "ASNOW", "surface", accum(0, hour))) {
                out.push_back({hour, w});
            }
            return out;
        };
        x.derive = [spread, meanOfMembers] (Grids& g, const Context&) {
            meanOfMembers(g, "s", "mean");
            auto out = g["mean"];
            if (spread) {
                for (size_t i = 0; i < out.values.size(); i++) {
                    double square = 0.0;
                    int n = 0;
                    for (int m = 1; m <= 5; m++) {
                        const float v = g.at("s" + std::to_string(m)).values[i];
                        if (!std::isnan(v)) {
                            square += (v - g["mean"].values[i]) * (v - g["mean"].values[i]);
                            n++;
                        }
                    }
                    out.values[i] = n > 0 ? static_cast<float>(std::sqrt(square / n)) : std::nanf("");
                }
            }
            g["f"] = GfsGrid::scaled(out, 100.0);   // the file's meters, as centimeters
        };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = snowfall();
        x.quantity = Quantity::Centimeters;
        x.fillTitle = spread ? "Spread of the snow since the start of the run (the 5 members)" : "Mean snow since the start of the run (the 5 members)";
        x.legendStep = 0;
        p.push_back(x);
    }
    // the thickness of a layer: the mean of the high level less the mean of the members' low one (the files have no 1000 mb mean)
    for (const auto& [id, label, low, high, interval, split] : {std::tuple{"ens_1000_500_thick", "Mean 1000-500mb Thickness", "1000 mb", "500 mb", 6.0, 540.0}, {"ens_1000_850_thick", "Mean 1000-850mb Thickness", "1000 mb", "850 mb", 3.0, 130.0},
                                                                 {"ens_850_700_thick", "Mean 850-700mb Thickness", "850 mb", "700 mb", 3.0, 154.0}}) {
        auto x = make(id, label);
        const std::string lowLevel = low, highLevel = high;
        x.needs = [ens, members, atHour, lowLevel, highLevel] (int hour) {
            std::vector<GfsData::Need> out;
            for (auto w : members("l", "HGT", lowLevel.c_str(), "")) {
                out.push_back({hour, w});
            }
            out.push_back({hour, ens("h", "HGT", highLevel.c_str(), atHour(hour), "mean", "wt ens mean")});
            return out;
        };
        x.derive = [meanOfMembers] (Grids& g, const Context&) {
            meanOfMembers(g, "l", "lm");
            g["thick"] = GfsGrid::scaled(GfsGrid::difference(g["h"], g["lm"]), 0.1);   // meters -> decameters
            g.erase("l1"); g.erase("l2"); g.erase("l3"); g.erase("l4"); g.erase("l5");
        };
        x.fill = [] (const Grids& g) { return pick(g, "thick"); };
        const double lo = split - 8 * interval, hi = split + 8 * interval;
        x.ramp = Ramp{{{lo, QColor{"#2b4fb0"}}, {lo + (hi - lo) * 0.25, QColor{"#5aa0d8"}}, {lo + (hi - lo) * 0.45, QColor{"#bfe3ee"}}, {split, QColor{"#f4f1c0"}}, {lo + (hi - lo) * 0.65, QColor{"#f5c06a"}},
                       {lo + (hi - lo) * 0.85, QColor{"#e0642c"}}, {hi, QColor{"#a01c1c"}}}};
        x.fillTitle = std::string{label} + " (dam)";
        x.legendStep = static_cast<double>(interval) * 2;
        ContourSet t;
        t.key = "thick";
        t.interval = interval;
        t.title = std::string{label} + " (dam)";
        t.color = QColor{190, 50, 40};
        t.colorBelow = QColor{40, 90, 190};
        t.split = split;
        t.width = 1.3;
        x.contours = {t};
        p.push_back(x);
    }
    {   // the mean 10 m wind (kt) with barbs, and the chance of 25 kt or more from the members
        auto x = make("ens_10m_wind", "Mean 10m Winds");
        x.needs = [ens, atHour] (int hour) {
            return std::vector<GfsData::Need>{{hour, ens("w", "WIND", "10 m above ground", atHour(hour), "mean", "wt ens mean")}, {hour, ens("u", "UGRD", "10 m above ground", atHour(hour), "mean", "wt ens mean")},
                                              {hour, ens("v", "VGRD", "10 m above ground", atHour(hour), "mean", "wt ens mean")}};
        };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "w"), 1.943844); };
        x.ramp = lowWind();
        x.fillTitle = "Mean 10 m wind speed (kt)";
        x.legendStep = 10;
        x.barbU = "u";
        x.barbV = "v";
        p.push_back(x);
    }
    {
        auto x = make("ens_prob_10m_wind_25kt", "Probability of 10m Wind Speeds over 25 kt");
        x.wants = members("u", "UGRD", "10 m above ground", "");
        for (auto& w : members("v", "VGRD", "10 m above ground", "")) {
            x.wants.push_back(w);
        }
        x.derive = [] (Grids& g, const Context&) {
            for (int m = 1; m <= 5; m++) {
                g["s" + std::to_string(m)] = GfsGrid::speed(g.at("u" + std::to_string(m)), g.at("v" + std::to_string(m)));
            }
        };
        x.fill = [fraction] (const Grids& g) { return fraction(g, "s", 25.0 * 0.514444); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance of a 10 m wind over 25 kt (% of the members, nearby)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("ens_2m_temp", "Mean 2m Temperature");
        x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("t", "TMP", "2 m above ground", atHour(hour), "mean", "wt ens mean")}}; };
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "Mean 2 m temperature";
        x.legendStep = 5;
        p.push_back(x);
    }
    {
        auto x = make("ens_prob_2m_temp_0C", "Probability of 2m Temperature under 0 C");
        x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("t", "TMP", "2 m above ground", atHour(hour), "prob", "prob <273.15")}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "t"), 100.0); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance of a 2 m temperature under 0 C (% of the members)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {
        auto x = make("ens_cape", "Mean Convective Available Potential Energy");
        x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("c", "CAPE", "surface", atHour(hour), "mean", "wt ens mean")}}; };
        x.fill = [] (const Grids& g) { return pick(g, "c"); };
        x.ramp = capeRamp();
        x.fillTitle = "Mean surface-based CAPE (J/kg)";
        x.legendStep = 500;
        p.push_back(x);
    }
    {
        auto x = make("ens_cin", "Mean Convective Inhibition");
        x.needs = [ens, atHour] (int hour) { return std::vector<GfsData::Need>{{hour, ens("c", "CIN", "surface", atHour(hour), "mean", "wt ens mean")}}; };
        x.fill = [] (const Grids& g) { return pick(g, "c"); };
        x.ramp = Ramp{{{-300, QColor{"#5b2a8a"}}, {-200, QColor{"#3a55b8"}}, {-100, QColor{"#5aa0d8"}}, {-25, QColor{"#dff3f8"}}, {0, QColor{223, 243, 248, 0}}}};
        x.fillTitle = "Mean surface-based CIN (J/kg)";
        x.legendStep = 50;
        p.push_back(x);
    }
    for (const int joules : {500, 1000, 2000}) {   // the ready-made chances are of the mixed-layer parcel
        auto x = make(("ens_prob_cape_" + std::to_string(joules)).c_str(), ("Probability of CAPE over " + std::to_string(joules)).c_str());
        const std::string detail = "prob >" + std::to_string(joules);
        x.needs = [ens, atHour, detail] (int hour) { return std::vector<GfsData::Need>{{hour, ens("c", "CAPE", "90-0 mb above ground", atHour(hour), "prob", detail.c_str())}}; };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "c"), 100.0); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance of mixed-layer CAPE over " + std::to_string(joules) + " J/kg (% of the members)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {   // the sea level pressure: the mean as lines, the spread as the fill
        auto x = make("ens_mslp", "Mean Sea Level Pressure and Spread");
        x.needs = [ens, atHour] (int hour) {
            return std::vector<GfsData::Need>{{hour, ens("p", "MSLET", "mean sea level", atHour(hour), "mean", "wt ens mean")}, {hour, ens("s", "MSLET", "mean sea level", atHour(hour), "sprd", "ens spread")}};
        };
        x.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "s"), 0.01); };
        x.ramp = spreadRamp(6.0);
        x.fillTitle = "Spread of the sea level pressure (mb)";
        x.legendStep = 1;
        x.contours = {pressure()};
        p.push_back(x);
    }
    // the mean of the upper-air fields at a level (height, wind, temperature, humidity): filled with what the map is about, heights as lines
    const auto meanUpper = [make, ens, atHour] (const char * id, const char * label, const char * level, double interval, bool temperatureToo, bool humidityToo) {
        auto x = make(id, label);
        const std::string where = level;
        x.needs = [ens, atHour, where, temperatureToo, humidityToo] (int hour) {
            std::vector<GfsData::Need> out{{hour, ens("z", "HGT", where.c_str(), atHour(hour), "mean", "wt ens mean")}, {hour, ens("u", "UGRD", where.c_str(), atHour(hour), "mean", "wt ens mean")},
                                           {hour, ens("v", "VGRD", where.c_str(), atHour(hour), "mean", "wt ens mean")}};
            if (temperatureToo) {
                out.push_back({hour, ens("t", "TMP", where.c_str(), atHour(hour), "mean", "wt ens mean")});
            }
            if (humidityToo) {
                out.push_back({hour, ens("rh", "RH", where.c_str(), atHour(hour), "mean", "wt ens mean")});
            }
            return out;
        };
        x.fill = speedOf("u", "v");
        x.ramp = windSpeed();
        x.fillTitle = "Mean wind speed (kt)";
        x.legendStep = 20;
        x.contours = {heights(interval)};
        x.barbU = "u";
        x.barbV = "v";
        return x;
    };
    for (const auto& [id, label, level, interval] : {std::tuple{"ens_250_vort_ht", "250mb Vorticity and Height", "250 mb", 12.0}, {"ens_500_vort_ht", "500mb Vorticity and Height", "500 mb", 6.0}}) {
        auto x = meanUpper(id, label, level, interval, false, false);
        x.fill = vorticityOf;
        x.ramp = vorticityRamp();
        x.fillTitle = "Vorticity of the mean wind (1e-5 /s)";
        x.legendStep = 10;
        x.barbU.clear();
        x.barbV.clear();
        p.push_back(x);
    }
    for (const auto& [id, label, level, interval] : {std::tuple{"ens_250_wnd", "250mb Wind", "250 mb", 12.0}, {"ens_850_wnd", "850mb Wind", "850 mb", 3.0}}) {
        p.push_back(meanUpper(id, label, level, interval, false, false));
    }
    for (const auto& [id, label, level, interval] : {std::tuple{"ens_700_temp", "700mb Temperature", "700 mb", 3.0}, {"ens_850_temp", "850mb Temperature", "850 mb", 3.0}}) {
        auto x = meanUpper(id, label, level, interval, true, false);
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "Mean temperature";
        x.legendStep = 5;
        p.push_back(x);
    }
    {
        auto x = meanUpper("ens_700_rh", "700mb Relative Humidity", "700 mb", 3.0, false, true);
        x.fill = [] (const Grids& g) { return pick(g, "rh"); };
        x.ramp = humidity();
        x.palette = "rh";
        x.fillTitle = "Mean relative humidity (%)";
        x.legendStep = 10;
        p.push_back(x);
    }
    {   // 850 mb: the file has no mean humidity there, so the mean of the members'
        auto x = meanUpper("ens_850_rh", "850mb Relative Humidity", "850 mb", 3.0, false, false);
        const auto inner = x.needs;
        x.needs = [inner, members] (int hour) {
            auto out = inner(hour);
            for (auto w : members("r", "RH", "850 mb", "")) {
                out.push_back({hour, w});
            }
            return out;
        };
        x.derive = [meanOfMembers] (Grids& g, const Context&) {
            meanOfMembers(g, "r", "rh");
            g.erase("r1"); g.erase("r2"); g.erase("r3"); g.erase("r4"); g.erase("r5");
        };
        x.fill = [] (const Grids& g) { return pick(g, "rh"); };
        x.ramp = humidity();
        x.palette = "rh";
        x.fillTitle = "Mean relative humidity (%)";
        x.legendStep = 10;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
