// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The chance of an amount of precipitation, for every ensemble: the share of the members whose total (or amount in a period) is at or over it.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

// ---- the chance of an amount of precipitation, for every ensemble: the share of the members whose total (or amount in a period) is at or over it. The bands are those of the commercial ensemble
// maps: white under 10 %, up to a deep red over 90 %.
const Ramp& exceedRamp() {
    static const Ramp chance{{{0.0, QColor{255, 255, 255}}, {10.0, QColor{93, 74, 168}}, {20.0, QColor{45, 120, 184}}, {30.0, QColor{70, 176, 160}}, {40.0, QColor{154, 212, 160}}, {50.0, QColor{244, 244, 176}},
                              {60.0, QColor{246, 216, 138}}, {70.0, QColor{248, 168, 90}}, {80.0, QColor{240, 105, 60}}, {90.0, QColor{140, 10, 58}}}, true};
    return chance;
}

// The chart for an amount (inches) and a period (hours; 0 the total since the start of the run): each member's total at the hour, less its total `period` hours before, and the share at or over the amount
Product exceedProduct(const CumulativeEnsemble& ensemble, const std::string& id, const std::string& label, double inches, int period) {
    const double mm = inches * 25.4 - 1e-6;
    Product x;
    x.source = ensemble.source;
    x.id = id;
    x.label = label;
    x.needs = [ensemble, period] (int hour) {
        std::vector<GfsData::Need> out;
        if (hour < 3 || (period > 0 && hour < period)) {
            return out;
        }
        for (const auto& name : ensemble.members) {
            out.push_back({hour, GfsData::Want{"q" + name, "APCP", "surface", ensemble.forecast(hour), "", name}});
            if (period > 0 && hour - period > 0) {   // the totals are from the start: the period's amount is the difference of two of them
                out.push_back({hour - period, GfsData::Want{"r" + name, "APCP", "surface", ensemble.forecast(hour - period), "", name}});
            }
        }
        return out;
    };
    x.derive = [ensemble, mm, period] (Grids& g, const Context& context) {
        auto out = g.at("q" + ensemble.members.front());
        const bool difference = period > 0 && context.hour - period > 0;
        for (size_t i = 0; i < out.values.size(); i++) {
            int n = 0, hits = 0;
            for (const auto& name : ensemble.members) {
                float v = g.at("q" + name).values[i];
                if (difference) {
                    v -= g.at("r" + name).values[i];
                }
                if (std::isfinite(v)) {
                    n++;
                    hits += v >= mm ? 1 : 0;
                }
            }
            out.values[i] = n > 0 ? static_cast<float>(100.0 * hits / n) : std::nanf("");
        }
        g["qp"] = std::move(out);
        for (const auto& name : ensemble.members) {
            g.erase("q" + name);
            g.erase("r" + name);
        }
    };
    x.fill = [] (const Grids& g) { return pick(g, "qp"); };
    x.ramp = exceedRamp();
    x.legendStep = 10;
    return x;
}

// the same for the members pooled from several ensembles (ENS-ALL): GfsRender reads each member's total, or its amount in the period, onto one grid ("q" + the member's tag)
Product exceedPooledProduct(const std::string& id, const std::string& label, double inches, int period) {
    const double mm = inches * 25.4 - 1e-6;
    Product x;
    x.source = "ENS-ALL";
    x.id = id;
    x.label = label;
    x.pool = {"APCP", "surface", "q"};
    x.pool.periodHours = period;
    x.derive = [mm] (Grids& g, const Context&) {
        std::vector<const GfsGrid::Grid *> members;
        for (const auto& member : poolMembers()) {
            const auto found = g.find("q" + member.tag);
            if (found != g.end() && !found->second.values.empty()) {
                members.push_back(&found->second);
            }
        }
        if (members.empty()) {
            throw std::runtime_error{"no member could be read"};
        }
        auto out = *members.front();
        for (size_t i = 0; i < out.values.size(); i++) {
            int n = 0, hits = 0;
            for (const auto * m : members) {
                const float v = m->values[i];
                if (std::isfinite(v)) {
                    n++;
                    hits += v >= mm ? 1 : 0;
                }
            }
            out.values[i] = n > 0 ? static_cast<float>(100.0 * hits / n) : std::nanf("");
        }
        g["qp"] = std::move(out);
        for (const auto& member : poolMembers()) {
            g.erase("q" + member.tag);
        }
    };
    x.fill = [] (const Grids& g) { return pick(g, "qp"); };
    x.ramp = exceedRamp();
    x.legendStep = 10;
    return x;
}

}  // namespace detail
}  // namespace GfsChart
