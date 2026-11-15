// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The mesoanalysis fields (SFCOA-HRRR and the RRFS's RRFSD).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void mesoProducts(std::vector<Product>& p, const char * source) {
    struct Spec {
        const char * id;
        const char * label;
        const char * variable;   // "name" or "u_name|v_name" for a vector drawn as its length
        double scale;
        double offset;           // value * scale + offset = the chart's unit
        Quantity quantity;
        const char * title;
        double step;
    };
    const Ramp cin{{{-300.0, QColor{"#4b1f6f"}}, {-200.0, QColor{"#6c3fa0"}}, {-100.0, QColor{"#4a78c8"}}, {-50.0, QColor{"#7fb8e6"}}, {-25.0, QColor{"#bfe4f2"}}, {-5.0, QColor{191, 228, 242, 0}}, {0.0, QColor{191, 228, 242, 0}}}};
    const Ramp index{{{0.0, QColor{0, 0, 40, 0}}, {0.5, QColor{0, 0, 40, 0}}, {1.0, QColor{60, 170, 90}}, {2.0, QColor{230, 220, 60}}, {4.0, QColor{240, 150, 40}}, {8.0, QColor{220, 40, 40}},
                      {12.0, QColor{170, 30, 150}}, {20.0, QColor{255, 255, 255}}}};
    const Ramp stp = Ramp{{{0.0, QColor{0, 0, 40, 0}}, {0.25, QColor{0, 0, 40, 0}}, {0.5, QColor{40, 60, 200}}, {1.0, QColor{60, 170, 90}}, {2.0, QColor{230, 220, 60}}, {3.0, QColor{240, 150, 40}},
                           {5.0, QColor{220, 40, 40}}, {8.0, QColor{170, 30, 150}}, {12.0, QColor{255, 255, 255}}}};
    const Ramp lapse{{{4.0, QColor{"#2f5aa8"}}, {5.5, QColor{"#7fb8e6"}}, {6.5, QColor{"#e8e0a0"}}, {7.5, QColor{"#f0a63a"}}, {8.5, QColor{"#d63a2a"}}, {10.0, QColor{"#7a1a6a"}}}};
    const Ramp height{{{0.0, QColor{"#2f5aa8"}}, {500.0, QColor{"#7fb8e6"}}, {1000.0, QColor{"#bfe4cf"}}, {1500.0, QColor{"#f1e27a"}}, {2000.0, QColor{"#f0a63a"}}, {3000.0, QColor{"#d63a2a"}}, {4500.0, QColor{"#7a1a6a"}}}};
    const Ramp angle{{{0.0, QColor{"#7a1a6a"}}, {30.0, QColor{"#d63a2a"}}, {60.0, QColor{"#f0a63a"}}, {90.0, QColor{"#7fcf7f"}}, {120.0, QColor{"#7fb8e6"}}, {180.0, QColor{"#2f5aa8"}}}};
    const Ramp mixing{{{0.0, QColor{"#a98a5a"}}, {4.0, QColor{"#dcd29a"}}, {8.0, QColor{"#a9d98a"}}, {12.0, QColor{"#4fb98a"}}, {16.0, QColor{"#2f8aa6"}}, {20.0, QColor{"#3a55b8"}}}};
    const std::vector<std::pair<Spec, Ramp>> specs{
        {{"sbcape", "Surface-Based CAPE", "sb_cape", 1.0, 0.0, Quantity::Other, "Surface-based CAPE (J/kg)", 500}, capeRamp()},
        {{"mlcape", "Mixed-Layer CAPE (lowest 100 mb)", "ml_cape", 1.0, 0.0, Quantity::Other, "Mixed-layer CAPE (J/kg)", 500}, capeRamp()},
        {{"mucape", "Most Unstable CAPE", "mu_cape", 1.0, 0.0, Quantity::Other, "Most unstable CAPE (J/kg)", 500}, capeRamp()},
        {{"sbcin", "Surface-Based CIN", "sb_cinh", 1.0, 0.0, Quantity::Other, "Surface-based CIN (J/kg)", 50}, cin},
        {{"mlcin", "Mixed-Layer CIN", "ml_cinh", 1.0, 0.0, Quantity::Other, "Mixed-layer CIN (J/kg)", 50}, cin},
        {{"mucin", "Most Unstable CIN", "mu_cinh", 1.0, 0.0, Quantity::Other, "Most unstable CIN (J/kg)", 50}, cin},
        {{"dcape", "Downdraft CAPE", "dcape", 1.0, 0.0, Quantity::Other, "Downdraft CAPE (J/kg)", 500}, capeRamp()},
        {{"srh_0_1km", "0-1 km Storm Relative Helicity", "srh_0_1km", 1.0, 0.0, Quantity::Other, "0-1 km storm relative helicity (m2/s2)", 0}, helicityRamp()},
        {{"srh_0_3km", "0-3 km Storm Relative Helicity", "srh_0_3km", 1.0, 0.0, Quantity::Other, "0-3 km storm relative helicity (m2/s2)", 0}, helicityRamp()},
        {{"srh_eff", "Effective Storm Relative Helicity", "srh_eff", 1.0, 0.0, Quantity::Other, "Effective storm relative helicity (m2/s2)", 0}, helicityRamp()},
        {{"stp_eff", "Significant Tornado Parameter (effective layer)", "stp_eff", 1.0, 0.0, Quantity::Other, "STP, effective layer: over 1, most significant tornadoes with a supercell", 1}, stp},
        {{"stp_fix", "Significant Tornado Parameter (fixed layer)", "stp_fix", 1.0, 0.0, Quantity::Other, "STP, fixed layer: over 1, most significant tornadoes with a supercell", 1}, stp},
        {{"scp", "Supercell Composite Parameter", "scp", 1.0, 0.0, Quantity::Other, "Supercell composite parameter (right movers)", 2}, index},
        {{"ship", "Significant Hail Parameter", "shp", 1.0, 0.0, Quantity::Other, "Significant hail parameter: over 1, significant hail", 1}, stp},
        {{"ehi_0_1km", "0-1 km Energy Helicity Index", "ehi_0_1km", 1.0, 0.0, Quantity::Other, "Energy helicity index, 0-1 km", 1}, index},
        {{"ehi_0_3km", "0-3 km Energy Helicity Index", "ehi_0_3km", 1.0, 0.0, Quantity::Other, "Energy helicity index, 0-3 km", 1}, index},
        {{"dcp", "Derecho Composite Parameter", "dcp", 1.0, 0.0, Quantity::Other, "Derecho composite parameter", 1}, index},
        {{"shear_0_6km", "0-6 km Bulk Wind Shear", "shear_0_6km_u|shear_0_6km_v", 1.943844, 0.0, Quantity::Other, "0-6 km bulk shear (kt)", 10}, shearRamp()},
        {{"shear_0_1km", "0-1 km Bulk Wind Shear", "shear_0_1km_u|shear_0_1km_v", 1.943844, 0.0, Quantity::Other, "0-1 km bulk shear (kt)", 10}, shearRamp()},
        {{"effective_shear", "Effective Bulk Wind Difference", "effective_shear_u|effective_shear_v", 1.943844, 0.0, Quantity::Other, "Effective bulk wind difference (kt)", 10}, shearRamp()},
        {{"mean_wind_0_6km", "0-6 km Mean Wind", "mean_wind_0_6km_u|mean_wind_0_6km_v", 1.943844, 0.0, Quantity::Other, "0-6 km mean wind (kt)", 10}, windSpeed()},
        {{"lapse_700_500", "700-500 mb Lapse Rate", "lapse_rate_700_500hPa", 1.0, 0.0, Quantity::Other, "700-500 mb lapse rate (K/km)", 1}, lapse},
        {{"lapse_0_3km", "0-3 km Lapse Rate", "lapse_rate_0_3km", 1.0, 0.0, Quantity::Other, "0-3 km lapse rate (K/km)", 1}, lapse},
        {{"temp_2m", "2 m Temperature", "tmpk_2m", 1.0, -273.15, Quantity::Temperature, "2 m temperature", 5}, temperature()},
        {{"dewpoint_2m", "2 m Dewpoint", "dwpk_2m", 1.0, -273.15, Quantity::Temperature, "2 m dewpoint", 5}, dewpoint()},
        {{"rh_2m", "2 m Relative Humidity", "relh_2m", 1.0, 0.0, Quantity::Other, "2 m relative humidity (%)", 10}, humidity()},
        {{"mixr_2m", "2 m Mixing Ratio", "mixr_2m", 1000.0, 0.0, Quantity::Other, "2 m water vapor mixing ratio (g/kg)", 2}, mixing},
        {{"thetae_2m", "2 m Equivalent Potential Temperature", "thetae_2m", 1.0, 0.0, Quantity::Other, "2 m equivalent potential temperature (K)", 10}, thetaERamp()},
        {{"pwat", "Precipitable Water", "pwat", 1.0, 0.0, Quantity::Other, "Precipitable water (mm)", 10}, precipitableWater()},
        {{"lcl_ml", "Mixed-Layer LCL Height", "ml_lcl_hght_agl", 1.0, 0.0, Quantity::Other, "Mixed-layer LCL height above ground (m)", 500}, height},
        {{"lfc_sb", "Surface-Based LFC Height", "sb_lfc_hght_agl", 1.0, 0.0, Quantity::Other, "Surface-based LFC height above ground (m)", 1000}, height},
        {{"fzl", "Freezing Level Height", "fzl_hght_agl", 1.0, 0.0, Quantity::Other, "Freezing level above ground (m)", 500}, height},
        {{"critical_angle", "Critical Angle", "critical_angle", 1.0, 0.0, Quantity::Other, "Critical angle (degrees; near 90 favors low level rotation)", 30}, angle},
    };
    for (const auto& [spec, ramp] : specs) {
        Product x;
        x.source = source;
        x.id = spec.id;
        x.label = spec.label;
        const std::string variable = spec.variable;
        const auto bar = variable.find('|');
        x.wants = {GfsData::Want{"a", variable.substr(0, bar), "", "", "", ""}};
        if (bar != std::string::npos) {
            x.wants.push_back(GfsData::Want{"b", variable.substr(bar + 1), "", "", "", ""});
        }
        const double scale = spec.scale, offset = spec.offset;
        const bool vector = bar != std::string::npos;
        x.fill = [scale, offset, vector] (const Grids& g) {
            auto out = pick(g, "a");
            const GfsGrid::Grid * second = vector ? &g.at("b") : nullptr;
            for (size_t i = 0; i < out.values.size(); i++) {
                const double v = vector ? std::hypot(static_cast<double>(out.values[i]), static_cast<double>(second->values[i])) : static_cast<double>(out.values[i]);
                out.values[i] = std::isfinite(v) ? static_cast<float>(v * scale + offset) : std::nanf("");
            }
            return out;
        };
        x.ramp = ramp;
        x.quantity = spec.quantity;
        x.fillTitle = spec.title;
        x.legendStep = spec.step;
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
