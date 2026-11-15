// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The waves of Meteo-France's model.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void mfWaveProducts(std::vector<Product>& p, const char * model) {
    struct Component {
        const char * name;
        const char * heightId;
        const char * directionId;
        const char * height;
        const char * period;
        const char * direction;
        const char * level;
    };
    for (const auto& c : {Component{"Significant Wave Height", "sig_wv_ht", "peak_dir_per", "HTSGW", "PERPW", "DIRPW", "surface"},
                          Component{"Wind Sea", "wsea_wv_ht", "wsea_dir_per", "WVHGT", "WVPER", "WVDIR", "surface"},
                          Component{"Primary Swell", "swell1_wv_ht", "swell1_dir_per", "SWELL", "SWPER", "SWDIR", "1 in sequence"},
                          Component{"Secondary Swell", "swell2_wv_ht", "swell2_dir_per", "SWELL", "SWPER", "SWDIR", "2 in sequence"}}) {
        {
            Product x;
            x.source = model;
            x.id = c.heightId;
            x.label = std::string{c.name};
            x.wants = {want("h", c.height, c.level)};
            x.fill = [] (const Grids& g) { return pick(g, "h"); };
            x.ramp = waveHeight();
            x.quantity = Quantity::Meters;
            x.fillTitle = std::string{c.name} + (std::string{c.heightId} == "sig_wv_ht" ? "" : " height");
            x.legendStep = 0;
            p.push_back(x);
        }
        {
            Product x;
            x.source = model;
            x.id = c.directionId;
            x.label = std::string{c.name} + " Direction and Period (sec)";
            x.wants = {want("t", c.period, c.level), want("d", c.direction, c.level)};
            x.derive = [] (Grids& g, const Context&) {   // the direction the waves come from, as a flow toward where they go
                auto u = g["d"], v = g["d"];
                for (size_t i = 0; i < u.values.size(); i++) {
                    const double from = g["d"].values[i] * pi / 180.0;
                    u.values[i] = std::isnan(g["d"].values[i]) ? std::nanf("") : static_cast<float>(-std::sin(from));
                    v.values[i] = std::isnan(g["d"].values[i]) ? std::nanf("") : static_cast<float>(-std::cos(from));
                }
                g["su"] = u;
                g["sv"] = v;
            };
            x.fill = [] (const Grids& g) { return pick(g, "t"); };
            x.ramp = wavePeriod();
            x.fillTitle = std::string{c.name} + " period (s); lines: direction the waves travel";
            x.legendStep = 2;
            x.streamU = "su";
            x.streamV = "sv";
            p.push_back(x);
        }
    }
}

}  // namespace detail
}  // namespace GfsChart
