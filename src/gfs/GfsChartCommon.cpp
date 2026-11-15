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

}  // namespace detail
}  // namespace GfsChart
