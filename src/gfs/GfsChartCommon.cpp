// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// What the chart catalogue files share that is more than a line: the number written plainly, and the table of the charts made for values the user chooses.
#include "gfs/GfsChartInternal.h"
#include <QString>

namespace GfsChart {
namespace detail {

// the number without trailing zeros: 2, 0.25
std::string trimmed(double value) {
    auto text = QString::number(value, 'f', 2);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
        text.chop(1);
    }
    return text.toStdString();
}

// template id -> a function making the product for the chosen values (filled when the registry is built)
std::map<std::string, std::function<Product(const std::vector<double>&)>>& generators() {
    static std::map<std::string, std::function<Product(const std::vector<double>&)>> table;
    return table;
}

std::string generatedId(const std::string& templateId, const std::vector<double>& values) {
    std::string id = "gen~" + templateId;
    for (const double v : values) {
        id += "~" + trimmed(v);
    }
    return id;
}

// equivalent potential temperature (Bolton 1980) in K from temperature in C, relative humidity in percent and the pressure in hPa
double thetaE(double tC, double rh, double p) {
    const double t = tC + 273.15;
    const double es = 6.112 * std::exp(17.67 * tC / (tC + 243.5));
    const double e = std::max(0.01, rh / 100.0 * es);
    const double td = 243.5 * std::log(e / 6.112) / (17.67 - std::log(e / 6.112)) + 273.15;
    const double tl = 1.0 / (1.0 / (td - 56.0) + std::log(t / td) / 800.0) + 56.0;
    const double r = 0.62197 * e / (p - e);   // kg/kg
    return t * std::pow(1000.0 / (p - e), 0.2854 * (1.0 - 0.28 * r)) * std::exp((3.376 / tl - 0.00254) * r * 1000.0 * (1.0 + 0.81 * r));
}

// The clouds by height in one map (the GFS, the HRRR and the RRFS give the cover of the low, middle and high layers): each place takes the layer with the most cover (a tie goes to the lower
// one, the layer seen from the ground), in that layer's colors, paler for less cover; under 20% clear. The low layer is the fill, the middle and the high are overlays with legend rows of their
// own; a place is in one grid only, so the colors never mix. The layers stand in for the cloud types: low the stratus and fog, middle the alto clouds, high the cirrus.
Product cloudLayersProduct(const std::string& model) {
    Product x;
    x.source = model;
    x.id = "cloud_layers";
    x.label = "Cloud Cover by Height (low, middle, high)";
    x.wants = {GfsData::Want{"lo", "LCDC", "low cloud layer", "", ""}, GfsData::Want{"mi", "MCDC", "middle cloud layer", "", ""}, GfsData::Want{"hi", "HCDC", "high cloud layer", "", ""}};
    x.derive = [] (Grids& g, const Context&) {
        if (g.find("lo") == g.end() || g.find("mi") == g.end() || g.find("hi") == g.end()) {
            return;
        }
        const auto& lo = g.at("lo");
        const auto& mi = g.at("mi");
        const auto& hi = g.at("hi");
        auto low = lo, middle = lo, high = lo;
        const float none = std::nanf("");
        for (size_t i = 0; i < low.values.size(); i++) {
            const float l = lo.values[i], m = i < mi.values.size() ? mi.values[i] : none, h = i < hi.values.size() ? hi.values[i] : none;
            const float lv = std::isnan(l) ? -1.0f : l, mv = std::isnan(m) ? -1.0f : m, hv = std::isnan(h) ? -1.0f : h;
            low.values[i] = middle.values[i] = high.values[i] = none;
            if (lv >= 20.0f && lv >= mv && lv >= hv) {
                low.values[i] = lv;
            } else if (mv >= 20.0f && mv >= hv && mv > lv) {
                middle.values[i] = mv;
            } else if (hv >= 20.0f && hv > lv && hv > mv) {
                high.values[i] = hv;
            }
        }
        g["low"] = std::move(low);
        g["fill|Middle cloud (%)"] = std::move(middle);
        g["fill|High cloud (%)"] = std::move(high);
    };
    x.fill = [] (const Grids& g) { return pick(g, "low"); };
    x.ramp = Ramp{{{20, QColor{"#ececec"}}, {50, QColor{"#bdbdbd"}}, {80, QColor{"#8a8a8a"}}, {100, QColor{"#5e5e5e"}}}};
    x.overlays = {{"fill|Middle cloud (%)", Ramp{{{20, QColor{"#dbe8f7"}}, {50, QColor{"#9cc0e8"}}, {80, QColor{"#4f8acb"}}, {100, QColor{"#245c9e"}}}}},
                  {"fill|High cloud (%)", Ramp{{{20, QColor{"#f8e1ef"}}, {50, QColor{"#eab0d3"}}, {80, QColor{"#d270ad"}}, {100, QColor{"#a23c82"}}}}}};
    x.fillTitle = "Low cloud (%): each place in the color of its layer with the most cover (gray low, blue middle, pink high; under 20% clear)";
    x.legendStep = 20;
    return x;
}

}  // namespace detail
}  // namespace GfsChart
