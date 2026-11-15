// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The charts of NASA's GEOS-FP: the dust, the aerosols, the fine particles and the smoke at the surface and in the column.
#include "gfs/GfsChartInternal.h"
#include <cmath>

namespace GfsChart {
namespace detail {

void geosProducts(std::vector<Product>& p) {
    const auto stretched = [] (const Ramp& base, double factor) {
        Ramp out = base;
        for (auto& stop : out.stops) {
            stop.first *= factor;
        }
        return out;
    };
    // micrograms per cubic meter of dust at the surface: nothing under 5, then yellows to browns to purple where a dust storm is
    const Ramp dust{{{0, QColor{246, 226, 150, 0}}, {5, QColor{246, 226, 150, 0}}, {10, QColor{"#f5dc8a"}}, {25, QColor{"#eab552"}}, {50, QColor{"#d98a3a"}}, {100, QColor{"#c0602a"}}, {250, QColor{"#9a3a25"}},
                     {500, QColor{"#6f1d3a"}}, {1000, QColor{"#4a0f5c"}}}};
    const Ramp optical{{{0, QColor{232, 243, 216, 0}}, {0.05, QColor{232, 243, 216, 0}}, {0.1, QColor{"#e8f3d8"}}, {0.25, QColor{"#cfe6a0"}}, {0.5, QColor{"#f1e27a"}}, {1.0, QColor{"#f0a63a"}},
                        {2.0, QColor{"#d63a2a"}}, {3.0, QColor{"#7a1a6a"}}}};
    const Ramp airQuality{{{0, QColor{0, 228, 0, 0}}, {4, QColor{0, 228, 0, 0}}, {12, QColor{"#00e400"}}, {35, QColor{"#ffff00"}}, {55, QColor{"#ff7e00"}}, {150, QColor{"#ff0000"}}, {250, QColor{"#8f3f97"}},
                           {500, QColor{"#7e0023"}}}};
    const Ramp smoke{{{0, QColor{217, 205, 184, 0}}, {2, QColor{217, 205, 184, 0}}, {5, QColor{"#d9cdb8"}}, {10, QColor{"#b8a58a"}}, {25, QColor{"#8c735a"}}, {50, QColor{"#5d4a3a"}}, {100, QColor{"#3a2c25"}},
                      {250, QColor{"#1c1410"}}}};
    struct Spec {
        const char * id;
        const char * label;
        std::vector<std::pair<const char *, double>> parts;   // variable and the factor to micrograms (or the unit) per cubic meter, added up
        double scale;                                         // kg/m3 to micrograms, kg/m2 to milligrams (1e6) or 1 for an optical thickness
        const Ramp * ramp;
        const char * title;
    };
    const std::vector<Spec> specs{
        {"dust_surface", "Dust: surface concentration", {{"DUSMASS", 1.0}}, 1e9, &dust, "Dust at the surface (micrograms per cubic meter)"},
        {"dust_pm25", "Dust: fine (PM2.5) surface concentration", {{"DUSMASS25", 1.0}}, 1e9, &dust, "Dust smaller than 2.5 micrometers at the surface (micrograms per cubic meter)"},
        {"dust_column", "Dust: column mass", {{"DUCMASS", 1.0}}, 1e6, nullptr, "Dust in the whole column (milligrams per square meter)"},
        {"dust_aod", "Dust: aerosol optical thickness", {{"DUEXTTAU", 1.0}}, 1.0, &optical, "Dust optical thickness at 550 nm (the amount of light it takes out; over 1 is thick)"},
        {"aod_total", "All aerosols: optical thickness", {{"TOTEXTTAU", 1.0}}, 1.0, &optical, "Total aerosol optical thickness at 550 nm"},
        {"pm25_total", "Fine particles (PM2.5), all species", {{"DUSMASS25", 1.0}, {"SSSMASS25", 1.0}, {"BCSMASS", 1.0}, {"OCSMASS", 1.4}, {"SO4SMASS", 1.375}}, 1e9, &airQuality,
         "PM2.5 at the surface: dust, sea salt, black and organic carbon, sulfate (micrograms per cubic meter)"},
        {"smoke_surface", "Smoke: black and organic carbon at the surface", {{"BCSMASS", 1.0}, {"OCSMASS", 1.4}}, 1e9, &smoke, "Black carbon plus organic matter at the surface (micrograms per cubic meter)"},
        {"sulfate_surface", "Sulfate at the surface", {{"SO4SMASS", 1.0}}, 1e9, &smoke, "Sulfate at the surface (micrograms per cubic meter)"},
        {"seasalt_surface", "Sea salt at the surface", {{"SSSMASS", 1.0}}, 1e9, &smoke, "Sea salt at the surface (micrograms per cubic meter)"},
    };
    for (const auto& spec : specs) {
        Product x;
        x.source = "GEOS-FP";
        x.id = spec.id;
        x.label = spec.label;
        std::vector<std::string> keys;
        for (size_t i = 0; i < spec.parts.size(); i++) {
            keys.push_back("a" + std::to_string(i));
            x.wants.push_back(GfsData::Want{keys.back(), spec.parts[i].first, "aerosol", "", ""});
        }
        x.wants.push_back(GfsData::Want{"u", "UGRD", "10 m above ground", "", ""});
        x.wants.push_back(GfsData::Want{"v", "VGRD", "10 m above ground", "", ""});
        const auto parts = spec.parts;
        const double scale = spec.scale;
        x.derive = [parts, keys, scale] (Grids& g, const Context&) {
            auto sum = g.at(keys.front());
            for (size_t i = 0; i < sum.values.size(); i++) {
                double total = 0.0;
                for (size_t k = 0; k < keys.size(); k++) {
                    total += static_cast<double>(g.at(keys[k]).values[i]) * parts[k].second;
                }
                sum.values[i] = std::isfinite(total) ? static_cast<float>(total * scale) : std::nanf("");
            }
            g["total"] = std::move(sum);
        };
        x.fill = [] (const Grids& g) { return pick(g, "total"); };
        x.ramp = spec.ramp ? *spec.ramp : stretched(dust, 2.0);   // the column in milligrams: twice the surface scale, roughly a kilometer and a half of mixed layer
        x.fillTitle = spec.title;
        x.legendStep = 0;
        x.barbU = "u";
        x.barbV = "v";
        x.periodHours = 3;   // the collection is 3 hour averages
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
