// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The ECMWF ensemble (IFS-ENS).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void ensProducts(std::vector<Product>& p) {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> out;
        for (int m = 1; m <= 50; m++) {
            out.push_back((m < 10 ? "e0" : "e") + std::to_string(m));
        }
        return out;
    }();
    const auto make = [] (const std::string& id, const std::string& label) {
        Product x;
        x.source = "IFS-ENS";
        x.id = id;
        x.label = label;
        return x;
    };
    // the same field from each member: keys "<prefix>e01" ...
    const auto memberNeeds = [] (const std::string& prefix, const char * variable, const char * level, int hour, const std::string& forecast) {
        std::vector<GfsData::Need> out;
        for (const auto& name : names) {
            out.push_back({hour, GfsData::Want{prefix + name, variable, level, forecast, "", name}});
        }
        return out;
    };
    // the mean and the spread (standard deviation) of the members' field: "<prefix>m" and "<prefix>s", in `scale` of the file's unit; the members are kept when `keep`
    const auto statistics = [] (Grids& g, const std::string& prefix, double scale, bool keep) {
        auto mean = g.at(prefix + names.front());
        auto spread = mean;
        for (size_t i = 0; i < mean.values.size(); i++) {
            double sum = 0.0, square = 0.0;
            int n = 0;
            for (const auto& name : names) {
                const float v = g.at(prefix + name).values[i];
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
        if (!keep) {
            for (const auto& name : names) {
                g.erase(prefix + name);
            }
        }
    };
    // spread as the fill, the mean as lines
    const auto spreadChart = [&] (const char * id, const char * label, const char * variable, const char * level, double scale, double top, const char * title, ContourSet lines) {
        auto x = make(id, label);
        x.needs = [memberNeeds, variable, level] (int hour) { return memberNeeds("f", variable, level, hour, hour == 0 ? std::string{"0 hour fcst"} : std::to_string(hour) + " hour fcst"); };
        x.derive = [statistics, scale] (Grids& g, const Context&) { statistics(g, "f", scale, false); };
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
        h.title = "Ensemble mean height (dam)";
        spreadChart("spread_500_ht", "500mb Height: ensemble mean and spread", "HGT", "500 mb", 0.1, 8.0, "Spread (standard deviation) of the 500 mb height among the 50 members (dam)", h);
        auto m = pressure();
        m.title = "Ensemble mean pressure (mb)";
        spreadChart("spread_mslp", "MSLP: ensemble mean and spread", "PRMSL", "mean sea level", 0.01, 8.0, "Spread of the sea level pressure among the 50 members (mb)", m);
        ContourSet t;
        t.interval = 5;
        t.title = "Ensemble mean 850 mb temperature (C)";
        t.color = QColor{30, 30, 30};
        spreadChart("spread_850_temp", "850mb Temperature: ensemble mean and spread", "TMP", "850 mb", 1.0, 5.0, "Spread of the 850 mb temperature among the 50 members (C)", t);
        ContourSet t2;
        t2.interval = 5;
        t2.title = "Ensemble mean 2 m temperature (C)";
        t2.color = QColor{30, 30, 30};
        spreadChart("spread_2m_temp", "2 m Temperature: ensemble mean and spread", "TMP", "2 m above ground", 1.0, 5.0, "Spread of the 2 m temperature among the 50 members (C)", t2);
    }
    // the chance of an amount of precipitation (the helpers above): the ready-made charts have the usual amounts; "Build a chart" has the amount (and the period) of the user's choosing
    {
        const CumulativeEnsemble ifs{"IFS-ENS", names, [] (int hour) { return "0-" + std::to_string(hour) + " hour acc fcst"; }};
        for (const auto& [id, inches, text] : {std::tuple{"ens_prob_total_0.01in", 0.01, "0.01"}, {"ens_prob_total_0.1in", 0.1, "0.10"}, {"ens_prob_total_0.25in", 0.25, "0.25"}, {"ens_prob_total_0.5in", 0.5, "0.50"},
                                               {"ens_prob_total_1in", 1.0, "1.00"}, {"ens_prob_total_2in", 2.0, "2.00"}}) {
            auto x = exceedProduct(ifs, id, std::string{"Probability of total precipitation of "} + text + " in or more (since the start of the run)", inches, 0);
            x.fillTitle = std::string{"Chance of "} + text + " in or more of precipitation in total since the start of the run (% of the 50 members)";
            p.push_back(x);
        }
        auto& made = generators();
        made["ens_total"] = [ifs] (const std::vector<double>& v) {   // inches
            const double inches = v.at(0);
            auto x = exceedProduct(ifs, generatedId("ens_total", {inches}), "Chance of " + trimmed(inches) + " in or more of precipitation in total (since the start of the run)", inches, 0);
            x.fillTitle = "Chance of " + trimmed(inches) + " in or more of precipitation in total since the start of the run (% of the 50 members)";
            return x;
        };
        made["ens_period"] = [ifs] (const std::vector<double>& v) {   // inches, hours
            const double inches = v.at(0);
            const int hours = std::max(3, static_cast<int>(std::lround(v.at(1) / 3.0)) * 3);
            auto x = exceedProduct(ifs, generatedId("ens_period", {inches, static_cast<double>(hours)}), "Chance of " + trimmed(inches) + " in or more of precipitation in " + std::to_string(hours) + " hours", inches, hours);
            x.fillTitleFor = [inches, hours] (int hour) {
                return "Chance of " + trimmed(inches) + " in or more of precipitation in the " + std::to_string(std::min(hours, hour)) + " hours ending at hour " + std::to_string(hour) + " (% of the 50 members)";
            };
            return x;
        };
    }
    // spaghetti: one contour (or two) of a field from every member, each its own color, and the ensemble mean as the heavy black line
    const auto spaghetti = [&] (const std::string& id, const std::string& label, const char * variable, const char * level, double scale, std::vector<double> levels, const std::string& what) {
        auto x = make(id, label);
        x.linesOnly = true;
        x.needs = [memberNeeds, variable, level] (int hour) { return memberNeeds("z", variable, level, hour, hour == 0 ? std::string{"0 hour fcst"} : std::to_string(hour) + " hour fcst"); };
        x.derive = [scale] (Grids& g, const Context&) {
            std::vector<const GfsGrid::Grid *> kept;   // (members taken out in the Layers box are not there)
            for (const auto& name : names) {
                if (const auto found = g.find("z" + name); found != g.end() && !found->second.values.empty()) {
                    kept.push_back(&found->second);
                }
            }
            if (kept.empty()) {
                throw std::runtime_error{"no member could be read"};
            }
            auto mean = *kept.front();
            for (size_t i = 0; i < mean.values.size(); i++) {
                double sum = 0.0;
                int n = 0;
                for (const auto * member : kept) {
                    const float v = member->values[i];
                    if (std::isfinite(v)) {
                        sum += v;
                        n++;
                    }
                }
                mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
            }
            g["zm"] = std::move(mean);
            (void) scale;
        };
        x.fillTitle = what + ": one thin line for each of the 50 members, the heavy black line the ensemble mean";
        for (const auto& name : names) {
            x.layers.push_back({"z" + name, "ECMWF member " + name});
        }
        for (const double value : levels) {
            for (size_t m = 0; m < names.size(); m++) {
                ContourSet c;
                c.key = "z" + names[m];
                c.scale = scale;
                c.base = value;
                c.interval = 100000.0;
                c.color = QColor::fromHsv(static_cast<int>(m * 360 / names.size()), 230, 190);
                c.width = 1.0;
                c.labelled = false;
                c.uniform = true;
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
            x.contours.push_back(mean);
        }
        p.push_back(x);
    };
    for (const auto& [low, high] : {std::pair{522, 564}, {528, 570}, {534, 576}, {540, 582}}) {
        spaghetti("spag_500_" + std::to_string(low) + "_" + std::to_string(high), "500mb " + std::to_string(low) + "/" + std::to_string(high) + " Height Contours (spaghetti)", "HGT", "500 mb", 0.1,
                  {static_cast<double>(low), static_cast<double>(high)}, "500 mb heights of " + std::to_string(low) + " and " + std::to_string(high) + " dam");
    }
    for (const auto& [low, high] : {std::pair{996, 1036}, {1000, 1040}, {1004, 1044}, {1008, 1048}}) {
        spaghetti("spag_mslp_" + std::to_string(low) + "_" + std::to_string(high), "MSLP " + std::to_string(low) + "/" + std::to_string(high) + " Isobar Contours (spaghetti)", "PRMSL", "mean sea level", 0.01,
                  {static_cast<double>(low), static_cast<double>(high)}, "sea level pressure of " + std::to_string(low) + " and " + std::to_string(high) + " mb");
    }
    // one member (the member picker chooses which; the first is the default)
    {
        auto x = make("member_2m_temp_mslp", "Member: 2 m Temperature and MSLP");
        x.wants = {GfsData::Want{"t", "TMP", "2 m above ground", "", "", "e01"}, GfsData::Want{"p", "PRMSL", "mean sea level", "", "", "e01"}};
        x.fill = [] (const Grids& g) { return pick(g, "t"); };
        x.ramp = temperature();
        x.quantity = Quantity::Temperature;
        x.fillTitle = "2 m temperature";
        x.legendStep = 5;
        x.contours = {pressure()};
        p.push_back(x);
        auto y = make("member_850_temp_mslp", "Member: 850mb Temperature and MSLP");
        y.wants = {GfsData::Want{"t", "TMP", "850 mb", "", "", "e01"}, GfsData::Want{"p", "PRMSL", "mean sea level", "", "", "e01"}};
        y.fill = [] (const Grids& g) { return pick(g, "t"); };
        y.ramp = temperature();
        y.quantity = Quantity::Temperature;
        y.fillTitle = "850 mb temperature";
        y.legendStep = 5;
        y.contours = {pressure()};
        p.push_back(y);
        auto z = make("member_500_ht", "Member: 500mb Height");
        z.wants = {GfsData::Want{"z", "HGT", "500 mb", "", "", "e01"}, GfsData::Want{"p", "PRMSL", "mean sea level", "", "", "e01"}};
        z.fill = [] (const Grids& g) { return GfsGrid::scaled(pick(g, "z"), 0.1); };
        z.ramp = Ramp{{{480, QColor{"#4b2d7f"}}, {510, QColor{"#3f74b8"}}, {540, QColor{"#9ad6e6"}}, {552, QColor{"#dcefb4"}}, {564, QColor{"#f3f0a4"}}, {576, QColor{"#f5ac5c"}}, {588, QColor{"#c5432e"}}, {600, QColor{"#8e1f2c"}}}};
        z.fillTitle = "500 mb height (dam)";
        z.legendStep = 12;
        z.contours = {heights(6.0)};
        p.push_back(z);
    }
}

}  // namespace detail
}  // namespace GfsChart
