// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// Meteo-France's ocean: the surface current.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void currentProducts(std::vector<Product>& p, const char * model) {
    {
        Product x;
        x.source = model;
        x.id = "ocean_current";
        x.label = "Surface Current";
        x.wants = {want("u", "UOGRD", "surface"), want("v", "VOGRD", "surface")};
        x.derive = [] (Grids& g, const Context&) {
            auto s = g["u"];
            for (size_t i = 0; i < s.values.size(); i++) {
                s.values[i] = static_cast<float>(std::hypot(g["u"].values[i], g["v"].values[i]));
            }
            g["s"] = s;
        };
        x.fill = [] (const Grids& g) { return pick(g, "s"); };
        x.ramp = currentSpeed();
        x.fillTitle = "Surface current speed (m/s); lines: where it flows";
        x.legendStep = 0;
        x.streamU = "u";
        x.streamV = "v";
        p.push_back(x);
    }
    {
        Product x;
        x.source = model;
        x.id = "sea_level_height";
        x.label = "Sea Surface Height";
        x.wants = {want("h", "SSHG", "surface")};
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = seaLevelHeight();
        x.quantity = Quantity::Meters;
        x.fillTitle = "Sea surface height above the mean";
        x.legendStep = 0;
        p.push_back(x);
    }
    {
        Product x;
        x.source = model;
        x.id = "inverse_barometer";
        x.label = "Sea Level Lift from Air Pressure";
        x.wants = {want("h", "IBHG", "surface")};
        x.fill = [] (const Grids& g) { return pick(g, "h"); };
        x.ramp = seaLevelHeight();
        x.quantity = Quantity::Meters;
        x.fillTitle = "Sea surface height the air pressure makes (a low pressure of 10 mb lifts the sea by 10 cm)";
        x.legendStep = 0;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
