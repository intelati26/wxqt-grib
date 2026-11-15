// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The air quality and UV charts of the Copernicus atmosphere service.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void camsProducts(std::vector<Product>& p, const char * model) {
    struct Chart {
        const char * id;
        const char * label;
        const char * variable;
        const char * unit;
        Ramp ramp;
        double step;
    };
    for (const auto& c : {Chart{"pm25", "PM2.5 fine particles", "PM25", "ug/m3", airQuality({9, 35.4, 55.4, 125.4, 225.4}), 0},
                          Chart{"pm10_aq", "PM10 particles", "PM10", "ug/m3", airQuality({54, 154, 254, 354, 424}), 0},
                          Chart{"dust", "Dust", "DUST", "ug/m3", airQuality({10, 50, 100, 250, 500}), 0},
                          Chart{"seasalt_aq", "Sea salt", "SALT", "ug/m3", airQuality({10, 25, 50, 100, 200}), 0},
                          Chart{"ozone_aq", "Ozone (surface)", "O3", "ug/m3", airQuality({100, 120, 150, 190, 240}), 0},
                          Chart{"no2_aq", "Nitrogen dioxide", "NO2", "ug/m3", airQuality({40, 100, 200, 400, 800}), 0},
                          Chart{"no_aq", "Nitrogen monoxide", "NO", "ug/m3", airQuality({10, 30, 60, 120, 250}), 0},
                          Chart{"so2_aq", "Sulphur dioxide", "SO2", "ug/m3", airQuality({20, 80, 250, 350, 500}), 0},
                          Chart{"co_aq", "Carbon monoxide", "CO", "ug/m3", airQuality({300, 1000, 2000, 5000, 10000}), 0},
                          Chart{"hcho_aq", "Formaldehyde", "HCHO", "ug/m3", airQuality({2, 5, 10, 20, 40}), 0},
                          Chart{"glyoxal_aq", "Glyoxal", "GLYX", "ug/m3", airQuality({0.05, 0.1, 0.2, 0.4, 0.8}), 0},
                          Chart{"pan_aq", "Peroxyacyl nitrates", "PAN", "ug/m3", airQuality({0.5, 1, 2, 4, 8}), 0},
                          Chart{"aod", "Aerosol optical depth (550 nm)", "AOD", "", airQuality({0.1, 0.2, 0.4, 0.8, 1.5}), 0},
                          Chart{"uv_index", "UV index", "UVI", "", uvIndex(), 1},
                          Chart{"uv_index_clear", "UV index, clear sky", "UVIC", "", uvIndex(), 1}}) {
        Product x;
        x.source = model;
        x.id = c.id;
        x.label = c.label;
        x.wants = {want("a", c.variable, "surface")};
        x.fill = [] (const Grids& g) { return pick(g, "a"); };
        x.ramp = c.ramp;
        x.fillTitle = std::string{c.label} + (*c.unit ? std::string{" ("} + c.unit + ")" : std::string{});
        x.legendStep = c.step;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
