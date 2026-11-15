// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The pooled ensembles (ENS-ALL): the ECMWF's, the GEFS and the GEPS members in one chart.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void ensAllProducts(std::vector<Product>& p) {
    const auto make = [] (const std::string& id, const std::string& label) {
        Product x;
        x.source = "ENS-ALL";
        x.id = id;
        x.label = label;
        return x;
    };
    // the mean and the spread of all the members present under a prefix, in `scale` of the file's unit
    const auto statistics = [] (Grids& g, const std::string& prefix, double scale) {
        std::vector<const GfsGrid::Grid *> members;
        for (const auto& member : poolMembers()) {
            const auto found = g.find(prefix + member.tag);
            if (found != g.end() && !found->second.values.empty()) {
                members.push_back(&found->second);
            }
        }
        if (members.empty()) {
            throw std::runtime_error{"no member could be read"};
        }
        auto mean = *members.front();
        auto spread = mean;
        for (size_t i = 0; i < mean.values.size(); i++) {
            double sum = 0.0, square = 0.0;
            int n = 0;
            for (const auto * m : members) {
                const float v = m->values[i];
                if (std::isfinite(v)) {
                    sum += v;
                    square += static_cast<double>(v) * v;
                    n++;
                }
            }
            mean.values[i] = n > 0 ? static_cast<float>(sum / n * scale) : std::nanf("");
            spread.values[i] = n > 1 ? static_cast<float>(std::sqrt(std::max(0.0, square / n - (sum / n) * (sum / n))) * scale) : std::nanf("");
        }
        g[prefix + "m"] = std::move(mean);
        g[prefix + "s"] = std::move(spread);
        for (const auto& member : poolMembers()) {   // the members are done with
            g.erase(prefix + member.tag);
        }
    };
    const auto spreadChart = [&] (const char * id, const char * label, const char * variable, const char * level, double scale, double top, const char * title, ContourSet lines) {
        auto x = make(id, label);
        x.pool = {variable, level, "f"};
        x.derive = [statistics, scale] (Grids& g, const Context&) { statistics(g, "f", scale); };
        x.fill = [] (const Grids& g) { return pick(g, "fs"); };
        x.ramp = spreadRamp(top);
        x.fillTitle = title;
        x.legendStep = 0;
        lines.key = "fm";
        lines.scale = 1.0;
        x.contours = {lines};
        p.push_back(x);
    };
    {
        auto h = heights(6.0);
        h.title = "Mean of all the members, height (dam)";
        spreadChart("all_spread_500_ht", "500mb Height: all ensembles, mean and spread", "HGT", "500 mb", 0.1, 8.0, "Spread (standard deviation) of the 500 mb height among the ECMWF, GEFS and GEPS members together (dam)", h);
        auto m = pressure();
        m.title = "Mean of all the members, pressure (mb)";
        spreadChart("all_spread_mslp", "MSLP: all ensembles, mean and spread", "PRMSL", "mean sea level", 0.01, 8.0, "Spread of the sea level pressure among all the members (mb)", m);
        ContourSet t;
        t.interval = 5;
        t.title = "Mean of all the members, 850 mb temperature (C)";
        t.color = QColor{30, 30, 30};
        spreadChart("all_spread_850_temp", "850mb Temperature: all ensembles, mean and spread", "TMP", "850 mb", 1.0, 5.0, "Spread of the 850 mb temperature among all the members (C)", t);
        ContourSet t2;
        t2.interval = 5;
        t2.title = "Mean of all the members, 2 m temperature (C)";
        t2.color = QColor{30, 30, 30};
        spreadChart("all_spread_2m_temp", "2 m Temperature: all ensembles, mean and spread", "TMP", "2 m above ground", 1.0, 5.0, "Spread of the 2 m temperature among all the members (C)", t2);
    }
    // spaghetti: a line from every member, each ensemble in a color family of its own (ECMWF reds, GEFS blues, GEPS greens), and the mean of all of them heavy and black
    const auto spaghetti = [&] (const std::string& id, const std::string& label, const char * variable, const char * level, double scale, std::vector<double> levels, const std::string& what) {
        auto x = make(id, label);
        x.linesOnly = true;
        x.pool = {variable, level, "z"};
        x.derive = [scale] (Grids& g, const Context&) {
            std::vector<const GfsGrid::Grid *> members;
            for (const auto& member : poolMembers()) {
                const auto found = g.find("z" + member.tag);
                if (found != g.end() && !found->second.values.empty()) {
                    members.push_back(&found->second);
                }
            }
            if (members.empty()) {
                throw std::runtime_error{"no member could be read"};
            }
            auto mean = *members.front();
            for (size_t i = 0; i < mean.values.size(); i++) {
                double sum = 0.0;
                int n = 0;
                for (const auto * m : members) {
                    if (std::isfinite(m->values[i])) {
                        sum += m->values[i];
                        n++;
                    }
                }
                mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
            }
            g["zm"] = std::move(mean);
            (void) scale;
        };
        x.fillTitle = what + ": a line from each of the 50 ECMWF (red), 31 GEFS (blue) and 21 GEPS (green) members, the heavy black line the mean of all";
        for (const auto& member : poolMembers()) {
            x.layers.push_back({"z" + member.tag, (member.source == "IFS-ENS" ? "ECMWF " : member.source + " ") + member.tag});
        }
        for (const double value : levels) {
            std::map<std::string, int> counts;
            for (const auto& member : poolMembers()) {
                ContourSet c;
                c.key = "z" + member.tag;
                c.scale = scale;
                c.base = value;
                c.interval = 100000.0;
                const int n = counts[member.source]++;
                const int span = member.source == "IFS-ENS" ? 50 : member.source == "GEFS" ? 31 : 21;
                const int hue = member.source == "IFS-ENS" ? 355 : member.source == "GEFS" ? 215 : 125;   // a color family for each ensemble, the members a shade apart
                c.color = QColor::fromHsv((hue + n * 28 / span) % 360, 200 + (n % 3) * 20, 170 + (n % 4) * 20);
                c.width = 0.9;
                c.labelled = false;
                c.uniform = true;
                if (value == levels.front() && n == 0) {
                    c.title = member.source == "IFS-ENS" ? "ECMWF ENS" : member.source == "GEFS" ? "GEFS" : "GEPS";
                }
                x.contours.push_back(c);
            }
            ContourSet mean;
            mean.key = "zm";
            mean.scale = scale;
            mean.base = value;
            mean.interval = 100000.0;
            mean.color = QColor{10, 10, 10};
            mean.width = 2.6;
            mean.uniform = true;
            if (value == levels.front()) {
                mean.title = "Mean of all";
            }
            x.contours.push_back(mean);
        }
        p.push_back(x);
    };
    for (const auto& [low, high] : {std::pair{522, 564}, {528, 570}, {534, 576}, {540, 582}}) {
        spaghetti("all_spag_500_" + std::to_string(low) + "_" + std::to_string(high), "500mb " + std::to_string(low) + "/" + std::to_string(high) + " Height Contours (spaghetti)", "HGT", "500 mb", 0.1,
                  {static_cast<double>(low), static_cast<double>(high)}, "500 mb heights of " + std::to_string(low) + " and " + std::to_string(high) + " dam");
    }
    for (const auto& [low, high] : {std::pair{996, 1036}, {1000, 1040}, {1004, 1044}, {1008, 1048}}) {
        spaghetti("all_spag_mslp_" + std::to_string(low) + "_" + std::to_string(high), "MSLP " + std::to_string(low) + "/" + std::to_string(high) + " Isobar Contours (spaghetti)", "PRMSL", "mean sea level", 0.01,
                  {static_cast<double>(low), static_cast<double>(high)}, "sea level pressure of " + std::to_string(low) + " and " + std::to_string(high) + " mb");
    }
    {   // the chance of an amount of precipitation among all the members: each member's total since the start of its own run, or its amount in the period ending at the valid time
        for (const auto& [id, inches, text] : {std::tuple{"all_prob_24h_0.01in", 0.01, "0.01"}, {"all_prob_24h_0.1in", 0.1, "0.10"}, {"all_prob_24h_0.25in", 0.25, "0.25"}, {"all_prob_24h_0.5in", 0.5, "0.50"},
                                               {"all_prob_24h_1in", 1.0, "1.00"}, {"all_prob_24h_2in", 2.0, "2.00"}}) {
            auto x = exceedPooledProduct(id, std::string{"Probability of "} + text + " in or more of precipitation in 24 hours: all ensembles", inches, 24);
            x.fillTitle = std::string{"Chance of "} + text + " in or more of precipitation in the 24 hours ending at the valid time (% of the 102 members of the ECMWF, GEFS and GEPS together)";
            p.push_back(x);
        }
        auto& made = generators();
        made["all_period"] = [] (const std::vector<double>& v) {   // inches, hours
            const double inches = v.at(0);
            const int hours = std::max(6, static_cast<int>(std::lround(v.at(1) / 6.0)) * 6);
            auto x = exceedPooledProduct(generatedId("all_period", {inches, static_cast<double>(hours)}), "Chance of " + trimmed(inches) + " in or more of precipitation in " + std::to_string(hours) + " hours: all ensembles", inches, hours);
            x.fillTitleFor = [inches, hours] (int hour) {
                return "Chance of " + trimmed(inches) + " in or more of precipitation in the " + std::to_string(std::min(hours, hour)) + " hours ending at hour " + std::to_string(hour) + " (% of all the members, ECMWF, GEFS and GEPS)";
            };
            return x;
        };
    }
}

}  // namespace detail
}  // namespace GfsChart
