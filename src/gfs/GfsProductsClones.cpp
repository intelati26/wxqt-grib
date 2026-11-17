// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The charts of a model that is read like the GFS (its files hold the same fields): the GFS charts it has, each cloned for that model with its own records, and the extras.
#include "gfs/GfsChartInternal.h"
#include <QTimeZone>
#include "gfs/GfsModels.h"

namespace GfsChart {
namespace detail {

void cloneFrom(std::vector<Product>& p, const GfsModels::Def& def, const std::map<std::string, Product>& plainPrecip) {
// ---- AIGFS: the same charts as the GFS, from its two files. Humidity comes as specific humidity (relative humidity is worked out from it with the temperature), and
// precipitation as 6 hour amounts (a period is the amounts of its 6 hour pieces added up).
const auto relativeHumidity = [] (const GfsGrid::Grid& temperature, const GfsGrid::Grid& specific, double hPa) {
    auto out = temperature;
    for (size_t i = 0; i < out.values.size(); i++) {
        const double t = temperature.values[i], q = specific.values[i];
        const double e = q * hPa / (0.622 + 0.378 * q);                       // the vapor pressure, hPa
        const double es = 6.112 * std::exp(17.67 * t / (t + 243.5));            // the saturation vapor pressure over water
        out.values[i] = static_cast<float>(std::clamp(100.0 * e / es, 0.0, 100.0));
    }
    return out;
};
const auto adapt = [relativeHumidity] (const Product& gfs, const char * model, bool specific, bool pieces = true) {
    Product x = gfs;
    x.source = model;
    if (x.source == "CFS" || x.source == "CFS-ANL") {   // the CFS charts' anomalies are from its own normal (cfsNormal)
        const std::string was = "from the 1991-2020 mean";
        if (const auto at = x.fillTitle.find(was); at != std::string::npos) {
            x.fillTitle.replace(at, was.size(), "from the CFS normal (1999-2010)");
        }
    }
    x.needs = [gfs, specific, pieces] (int hour) {
        std::vector<GfsData::Need> out;
        int end = -1, start = 0;
        for (auto need : GfsChart::needs(gfs, hour)) {
            if (pieces && need.want.variable == "APCP") {   // the running total is not in the file: its pieces are taken below
                (need.want.key == "a0" ? start : end) = need.hour;
                continue;
            }
            if (specific && need.want.variable == "RH") {     // the temperature and the specific humidity of that level, the level kept in the keys
                const auto key = need.want.key + "|" + need.want.level;
                auto t = need, q = need;
                t.want.variable = "TMP";
                t.want.key = key + "|t";
                q.want.variable = "SPFH";
                q.want.key = key + "|q";
                out.push_back(t);
                out.push_back(q);
                continue;
            }
            out.push_back(std::move(need));
        }
        if (pieces && end > 0) {
            int n = 0;
            for (int e = end; e > start && e > 0; e -= 6) {
                out.push_back({e, {"w" + std::to_string(n++), "APCP", "surface", std::to_string(e - 6) + "-" + std::to_string(e) + " hour acc fcst", ""}});
            }
        }
        return out;
    };
    x.fallbackNeeds = nullptr;
    x.derive = [gfs, relativeHumidity] (Grids& g, const Context& context) {
        // relative humidity from each level's pair
        std::vector<std::string> keys;
        for (const auto& [key, grid] : g) {
            const auto bar = key.find('|');
            if (bar != std::string::npos && key.size() > 2 && key.compare(key.size() - 2, 2, "|q") == 0) {
                keys.push_back(key.substr(0, key.size() - 2));
            }
        }
        for (const auto& key : keys) {
            const auto bar = key.find('|');
            const double hPa = std::atof(key.c_str() + bar + 1);   // "rh|500 mb" -> 500
            g[key.substr(0, bar)] = relativeHumidity(g[key + "|t"], g[key + "|q"], hPa);
        }
        // the precipitation of the period: its 6 hour pieces added up, as the running total less nothing (the pieces start where the period does)
        if (g.count("w0")) {
            auto sum = g["w0"];
            for (int i = 1; g.count("w" + std::to_string(i)); i++) {
                const auto& piece = g["w" + std::to_string(i)];
                for (size_t k = 0; k < sum.values.size(); k++) {
                    sum.values[k] += piece.values[k];
                }
            }
            g["a1"] = std::move(sum);
        }
        if (gfs.derive) {
            gfs.derive(g, context);
        }
    };
    if (pieces && gfs.fillTitleFor) {   // the pieces are 6 hours: the period is told from them
        x.fillTitleFor = [x, gfs] (int hour) {
            int lowest = hour;
            for (const auto& need : x.needs(hour)) {
                if (need.want.variable == "APCP") {
                    lowest = std::min(lowest, need.hour - 6);
                }
            }
            const auto original = gfs.fillTitleFor(hour);
            const auto at = original.find(", hours ");
            return (at == std::string::npos ? original : original.substr(0, at)) + ", hours " + std::to_string(std::max(0, lowest)) + "-" + std::to_string(hour);
        };
    }
    return x;
};
// The clone: a model's charts made from the GFS ones as its registry entry says (GfsModels.cpp): the whole list, or only the charts whose fields the model has.
    const auto& rule = def.clone;
    const auto addChart = [&] (const Product& original) {
        auto x = adapt(original, def.id.c_str(), rule.specificHumidity, rule.pieces);
        if (!rule.rename.empty()) {
            const auto inner = x.needs;
            const auto rename = rule.rename;
            x.needs = [inner, rename] (int hour) {
                auto list = inner(hour);
                for (auto& need : list) {
                    const auto found = rename.find(need.want.variable);
                    if (found != rename.end()) {
                        need.want.variable = found->second;
                    }
                }
                return list;
            };
        }
        x.label = rule.labelPrefix + x.label;
        p.push_back(std::move(x));
    };
    const auto fits = [&] (const Product& chart) {
        if (chart.cloneTo && !chart.cloneTo(def.id)) {
            return false;
        }
        for (const auto& part : rule.skip) {
            if (chart.id.find(part) != std::string::npos) {
                return false;
            }
        }
        for (const auto& need : GfsChart::needs(chart, 24)) {
            const auto& w = need.want;
            const bool pressureLevel = w.level.size() > 3 && w.level.compare(w.level.size() - 3, 3, " mb") == 0 && w.level.find("above ground") == std::string::npos;
            if ((!rule.variables.empty() && !rule.variables.count(w.variable)) || (pressureLevel && !rule.levels.empty() && !rule.levels.count(w.level))) {
                return false;
            }
            if (pressureLevel && !rule.fields.empty() && !rule.fields.count(w.variable + "|" + w.level)) {
                return false;
            }
        }
        return true;
    };
    const auto plainOf = [&] (const Product& chart) -> const Product& {   // the chart before the lines were put on it, when the model's maps have none
        for (const auto& prefix : rule.plain) {
            const auto found = plainPrecip.find(chart.id);
            if (chart.id.compare(0, prefix.size(), prefix) == 0 && found != plainPrecip.end()) {
                return found->second;
            }
        }
        return chart;
    };
    const auto size = p.size();
    if (!rule.ids.empty()) {
        for (const auto& id : rule.ids) {
            for (size_t i = 0; i < size; i++) {
                if (p[i].id == id && p[i].source == "GFS") {
                    const auto chart = plainOf(p[i]);
                    if (fits(chart)) {
                        addChart(chart);
                    }
                    break;
                }
            }
        }
    } else {
        for (size_t i = 0; i < size; i++) {
            if (p[i].source == "GFS") {
                const auto chart = plainOf(p[i]);
                if (fits(chart)) {
                    addChart(chart);
                }
            }
        }
    }
}

}  // namespace detail
}  // namespace GfsChart
