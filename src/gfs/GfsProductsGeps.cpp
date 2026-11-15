// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The Canadian ensemble (GEPS).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void gepsProducts(std::vector<Product>& p) {
    const auto spreadOf = [] (const char * id, const char * label, const char * variable, const char * level, double top, double step, const char * title, Quantity quantity) {
        Product x;
        x.source = "GEPS";
        x.id = id;
        x.label = label;
        x.wants = {GfsData::Want{"s", variable, level, "", "", "spr"}};
        x.fill = [] (const Grids& g) { return pick(g, "s"); };
        x.ramp = spreadRamp(top);
        x.quantity = quantity;
        x.fillTitle = title;
        x.legendStep = step;
        return x;
    };
    p.push_back(spreadOf("spread_2m_temp", "2m Temperature Spread among the members", "TMP", "2 m above ground", 6.0, 1.0, "Spread of the 2 m temperature (C)", Quantity::Other));
    p.push_back(spreadOf("spread_mslp", "Sea Level Pressure Spread among the members", "PRMSL", "mean sea level", 600.0, 100.0, "Spread of the sea level pressure (Pa)", Quantity::Other));
    for (const char * level : {"500", "850"}) {
        const std::string l = std::string{level} + " mb";
        p.push_back(spreadOf((std::string{"spread_"} + level + "_ht").c_str(), (std::string{level} + "mb Height Spread among the members").c_str(), "HGT", l.c_str(), std::string{level} == "500" ? 60.0 : 30.0,
                             std::string{level} == "500" ? 10.0 : 5.0, ("Spread of the " + std::string{level} + " mb height (m)").c_str(), Quantity::Other));
    }
    // the share of the 21 members at or under / over a temperature, each member its own fetch of the field (the cache keeps them)
    const auto chance = [&] (const char * id, const char * label, double limit, bool under, const char * title) {
        Product x;
        x.source = "GEPS";
        x.id = id;
        x.label = label;
        for (int m = 0; m < 21; m++) {
            const std::string name = m == 0 ? "c00" : "p" + std::string{m < 10 ? "0" : ""} + std::to_string(m);
            x.wants.push_back(GfsData::Want{"t" + std::to_string(m), "TMP", "2 m above ground", "", "", name});
        }
        x.fill = [limit, under] (const Grids& g) {
            auto out = pick(g, "t0");
            for (size_t i = 0; i < out.values.size(); i++) {
                int n = 0, hit = 0;
                for (int m = 0; m < 21; m++) {
                    const float v = g.at("t" + std::to_string(m)).values[i];
                    if (!std::isnan(v)) {
                        n++;
                        hit += (under ? v <= limit : v >= limit) ? 1 : 0;
                    }
                }
                out.values[i] = n > 0 ? static_cast<float>(100.0 * hit / n) : std::nanf("");
            }
            return out;
        };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = title;
        x.legendStep = 10;
        p.push_back(x);
    };
    chance("prob_2m_temp_freezing", "Probability of 2m Temperature at or under Freezing", 0.0, true, "Chance of 0 C or colder (% of the 21 members)");
    chance("prob_2m_temp_90f", "Probability of 2m Temperature at or over 90 F", (90.0 - 32.0) * 5.0 / 9.0, false, "Chance of 90 F or hotter (% of the 21 members)");
    {   // the chance of an amount of precipitation: each member's running total since the start of the run (the file of an hour holds the total up to it)
        std::vector<std::string> members;
        for (int m = 0; m < 21; m++) {
            members.push_back(m == 0 ? "c00" : "p" + std::string{m < 10 ? "0" : ""} + std::to_string(m));
        }
        const CumulativeEnsemble geps{"GEPS", members, [] (int) { return std::string{"0-*"}; }};
        for (const auto& [id, inches, text] : {std::tuple{"prob_total_0.01in", 0.01, "0.01"}, {"prob_total_0.1in", 0.1, "0.10"}, {"prob_total_0.25in", 0.25, "0.25"}, {"prob_total_0.5in", 0.5, "0.50"},
                                               {"prob_total_1in", 1.0, "1.00"}, {"prob_total_2in", 2.0, "2.00"}}) {
            auto x = exceedProduct(geps, id, std::string{"Probability of total precipitation of "} + text + " in or more (since the start of the run)", inches, 0);
            x.fillTitle = std::string{"Chance of "} + text + " in or more of precipitation in total since the start of the run (% of the 21 members)";
            p.push_back(x);
        }
        auto& made = generators();
        made["geps_total"] = [geps] (const std::vector<double>& v) {
            const double inches = v.at(0);
            auto x = exceedProduct(geps, generatedId("geps_total", {inches}), "Chance of " + trimmed(inches) + " in or more of precipitation in total (since the start of the run)", inches, 0);
            x.fillTitle = "Chance of " + trimmed(inches) + " in or more of precipitation in total since the start of the run (% of the 21 members)";
            return x;
        };
        made["geps_period"] = [geps] (const std::vector<double>& v) {
            const double inches = v.at(0);
            const int hours = std::max(3, static_cast<int>(std::lround(v.at(1) / 3.0)) * 3);
            auto x = exceedProduct(geps, generatedId("geps_period", {inches, static_cast<double>(hours)}), "Chance of " + trimmed(inches) + " in or more of precipitation in " + std::to_string(hours) + " hours", inches, hours);
            x.fillTitleFor = [inches, hours] (int hour) {
                return "Chance of " + trimmed(inches) + " in or more of precipitation in the " + std::to_string(std::min(hours, hour)) + " hours ending at hour " + std::to_string(hour) + " (% of the 21 members)";
            };
            return x;
        };
    }
}

}  // namespace detail
}  // namespace GfsChart
