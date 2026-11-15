// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The sea surface temperature of the Meteo-France ocean system.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void sstProducts(std::vector<Product>& p, const char * model) {
    Product x;
    x.source = model;
    x.id = "sst";
    x.label = "Sea Surface Temperature";
    x.wants = {want("t", "WTMP", "surface")};
    x.fill = [] (const Grids& g) { return pick(g, "t"); };
    x.ramp = seaSurfaceTemperature();
    x.quantity = Quantity::Temperature;
    x.fillTitle = "Sea surface temperature";
    x.legendStep = 2;
    p.push_back(x);
}

}  // namespace detail
}  // namespace GfsChart
