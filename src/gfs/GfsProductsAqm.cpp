// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The charts of NOAA's air quality forecast model (AQMv7): the hourly mean of fine particles and of ozone at the surface, as the model has them and bias-corrected with AirNow's observations.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void aqmProducts(std::vector<Product>& p, const char * model) {
    struct Chart {
        const char * id;
        const char * label;
        const char * variable;
        const char * stat;   // "bc": the bias-corrected product
        const char * unit;
        Ramp ramp;
    };
    for (const auto& c : {Chart{"pm25_aqm", "PM2.5 fine particles, hourly mean", "PMTF", "", "ug/m3", airQuality({9, 35.4, 55.4, 125.4, 225.4})},
                          Chart{"pm25_aqm_bc", "PM2.5 fine particles, hourly mean, bias-corrected with AirNow", "PMTF", "bc", "ug/m3", airQuality({9, 35.4, 55.4, 125.4, 225.4})},
                          Chart{"ozone_aqm", "Ozone, hourly mean", "OZCON", "", "ppb", airQuality({54, 70, 85, 105, 200})},
                          Chart{"ozone_aqm_bc", "Ozone, hourly mean, bias-corrected with AirNow", "OZCON", "bc", "ppb", airQuality({54, 70, 85, 105, 200})}}) {
        Product x;
        x.source = model;
        x.id = c.id;
        x.label = c.label;
        x.wants = {GfsData::Want{"a", c.variable, "1 sigma level", "", "", c.stat}};
        x.fill = [] (const Grids& g) { return pick(g, "a"); };
        x.ramp = c.ramp;
        x.fillTitle = std::string{c.label} + " (" + c.unit + ")";
        x.legendStep = 0;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
