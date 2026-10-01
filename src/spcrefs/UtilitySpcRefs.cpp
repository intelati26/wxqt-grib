// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spcrefs/UtilitySpcRefs.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <QBuffer>
#include <QFont>
#include <QLineF>
#include <QPainter>
#include <QRegularExpression>
#include "objects/URL.h"
#include "radar/RadarGeomInfo.h"
#include "radar/RadarGeometryTypeEnum.h"
#include "spcrefs/LambertGrid.h"

namespace UtilitySpcRefs {
    namespace {
        using StopList = vector<Stop>;
        const std::map<int, const char *> noNames;

        Stop hex(double value, unsigned rgb) {
            return {value, static_cast<int>((rgb >> 16) & 255), static_cast<int>((rgb >> 8) & 255), static_cast<int>(rgb & 255)};
        }

        // thresholds and the colours of the bands between them (one fewer): SPC's colour tables are written that way
        StopList bands(const vector<double>& thresholds, const vector<unsigned>& colours) {
            StopList list;
            for (size_t i = 0; i < colours.size() && i < thresholds.size(); i += 1) {
                list.push_back(hex(thresholds[i], colours[i]));
            }
            return list;
        }

        StopList reflectivityStops() {   // dBZ, SPC's table
            vector<double> t;
            for (double v = 10.0; v <= 70.0 + 1e-9; v += 2.5) {
                t.push_back(v);
            }
            return bands(t, {0xdae2f1, 0xb5c4e3, 0x90a7d6, 0x6a89c8, 0x476cbb, 0x274fae, 0x254f5d, 0x4e764d, 0x7e9a73, 0xadbf8f, 0xfdf384,
                             0xf0d46e, 0xe4b559, 0xd99844, 0xcd7933, 0xc35c24, 0xbb271a, 0x931d15, 0x6c1413, 0x933ea8, 0x772c9b, 0x59198d,
                             0x828282, 0xb4b4b4});
        }

        StopList qpfStops() {   // inches, SPC's table
            return bands({0.01, 0.1, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 5, 7, 10, 15, 20},
                         {0xc7e7c0, 0xa1e39b, 0x74c476, 0x31a353, 0x006d2c, 0xfffa8a, 0xffcc4f, 0xfe8d3c, 0xfc4e2a, 0xd61a1c, 0xad0026, 0x690000,
                          0xffaafa, 0xff83f9, 0xff57f7, 0xe619f9, 0x9400cc});
        }

        StopList snowStops() {
            return bands({0.1, 0.5, 1, 2, 3, 4, 6, 8, 12, 18, 24}, {0xc6dbef, 0x9ecae1, 0x6baed6, 0x4292c6, 0x2171b5, 0x08519c, 0x08306b, 0x6a51a3, 0x54278f, 0x3f007d});
        }

        StopList freezingRainStops() {
            return bands({0.01, 0.05, 0.1, 0.2, 0.3}, {0xea3729, 0xb92c1f, 0x892214, 0x581b08});
        }

        StopList iceAccretionStops() {
            return bands({0.01, 0.02, 0.03, 0.04, 0.05, 0.09, 0.13, 0.17, 0.21, 0.25, 0.3, 0.35, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 2.0},
                         {0xdcdcdc, 0xbebebe, 0x9f9f9f, 0x808080, 0xedc4cc, 0xefabb0, 0xf09296, 0xf2797a, 0xf3605f, 0xc0536e, 0xab516e, 0x974f6f,
                          0x824c6f, 0x59407b, 0x443d74, 0x2f3b6c, 0x1a3866, 0x04355e, 0x004b59});
        }

        StopList probabilityStops() {   // percent
            return bands({10, 20, 30, 40, 50, 60, 70, 80, 90},
                         {0xc6dbef, 0x6baed6, 0x2171b5, 0x08306b, 0xc7e9c0, 0x74c476, 0x238b45, 0xfee391, 0xfe9929});
        }

        StopList updraftHelicityStops() {   // m2/s2
            return bands({25, 40, 50, 60, 75, 100, 125, 150, 200, 250, 300, 400, 500},
                         {0xbebebe, 0x787878, 0x96bef0, 0x5a8ce6, 0x96dc96, 0x3caa50, 0x007800, 0xfff078, 0xf0c828, 0xe69600, 0xd296e6, 0x9646b4, 0x641482});
        }

        StopList helicityStops() {   // storm-relative helicity, m2/s2
            return bands({50, 100, 150, 200, 300, 400, 500}, {0xa1d99b, 0xffff66, 0xfdae61, 0xf46d43, 0xd7191c, 0x9e0142, 0x542788});
        }

        StopList capeStops() {   // J/kg
            return bands({100, 250, 500, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 6000}, {0xc6dbef, 0x9ecae1, 0x74c476, 0xfff078, 0xfdd24a, 0xfdae61, 0xf46d43, 0xd7191c, 0xae017e, 0x6a3d9a});
        }

        StopList cinStops() {   // magnitude, J/kg
            return bands({25, 50, 75, 100, 150, 200, 300}, {0xdeebf7, 0xc6dbef, 0x9ecae1, 0x6baed6, 0x3182bd, 0x08519c, 0x08306b});
        }

        StopList windSpeedStops(double scale) {   // knots (scale 1) or mph (1.15)
            return bands({10 * scale, 15 * scale, 20 * scale, 25 * scale, 30 * scale, 35 * scale, 40 * scale, 50 * scale, 60 * scale, 70 * scale, 80 * scale},
                         {0xdeebf7, 0xc6dbef, 0x9ecae1, 0x74c476, 0xfff078, 0xfdd24a, 0xfdae61, 0xf46d43, 0xd7191c, 0xae017e, 0x6a3d9a});
        }

        StopList updraftSpeedStops() {   // m/s
            return bands({10, 15, 20, 25, 30, 35, 40, 50, 60}, {0xc6dbef, 0x9ecae1, 0x74c476, 0xfff078, 0xfdae61, 0xf46d43, 0xd7191c, 0xae017e, 0x6a3d9a});
        }

        StopList stpStops() {
            return bands({0.5, 1, 2, 3, 4, 6, 8, 10}, {0xc7e9c0, 0xfff078, 0xfdae61, 0xf46d43, 0xd7191c, 0xae017e, 0x6a3d9a, 0x3f007d});
        }

        StopList rainbow(double lo, double hi) {
            static const unsigned colours[] = {0x6a3d9a, 0x3b4cc0, 0x00a0ff, 0x00c8a0, 0x60d040, 0xe6e61e, 0xffa000, 0xe03020, 0x8c0a0a};
            StopList list;
            for (int i = 0; i < 9; i += 1) {
                list.push_back(hex(lo + (hi - lo) * i / 8.0, colours[i]));
            }
            return list;
        }

        StopList temperatureStops(bool fahrenheit) {
            // degrees F (converted for C): cold purples through blues and greens to yellows, oranges and reds
            const vector<std::pair<double, unsigned>> f{{-30, 0xffffff}, {-10, 0x9a64c8}, {0, 0x6496ff}, {20, 0x00b4ff}, {32, 0x00c864},
                                                        {50, 0x96dc00}, {70, 0xffff00}, {85, 0xffa000}, {100, 0xff0000}, {115, 0x960000}};
            StopList list;
            for (const auto& [value, colour] : f) {
                list.push_back(hex(fahrenheit ? value : (value - 32.0) / 1.8, colour));
            }
            return list;
        }

        StopList relativeHumidityStops() {   // percent; SPC's table
            vector<double> t{0, 2.5, 5, 7.5, 10, 12.5, 15, 17.5, 20, 22.5, 25, 27.5, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100};
            return bands(t, {0x681813, 0x74231e, 0x983e3a, 0xbe5b58, 0xc39172, 0x9d745e, 0x765849, 0x503b34, 0x565656, 0x828282, 0xadadad, 0xd8d8d8,
                             0xccfdb9, 0x8ec17f, 0x528644, 0x1e4b10, 0x6f9eab, 0x59858d, 0x436a6f, 0x2e5050, 0x666695, 0x545084, 0x433b73, 0x312662, 0x73476e, 0x7e5870});
        }

        StopList precipitableWaterStops() {   // inches; SPC's table
            return bands({0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 2.75, 3.0},
                         {0xaf9f83, 0x8f7d69, 0x6f5c4e, 0xdfeaac, 0xb8e8a1, 0x8ebd80, 0x64935f, 0x3c693e, 0x6f9eab, 0x5e8b94, 0x4f787e});
        }

        StopList cloudStops(unsigned colour) {
            const int r = (colour >> 16) & 255;
            const int g = (colour >> 8) & 255;
            const int b = colour & 255;
            StopList list;
            for (int i = 0; i <= 8; i += 1) {
                const double t = i / 8.0;
                list.push_back({10.0 + 90.0 * t, static_cast<int>(240 + (r - 240) * t), static_cast<int>(240 + (g - 240) * t), static_cast<int>(240 + (b - 240) * t)});
            }
            return list;
        }

        StopList componentStops() {   // wind components, knots: blue (negative) to red (positive)
            return {hex(-40, 0x2166ac), hex(-20, 0x67a9cf), hex(-5, 0xd1e5f0), hex(0, 0xf7f7f7), hex(5, 0xfddbc7), hex(20, 0xef8a62), hex(40, 0xb2182b)};
        }

        // SPC's own titles for its main products (https://www.spc.noaa.gov/exper/refs/viewer), in its menu order
        struct Curated {
            const char * id;
            const char * title;
            const char * group;
        };
        const char * severeGroup = "Severe storms";
        const char * instabilityGroup = "Instability and shear";
        const char * precipitationGroup = "Precipitation";
        const char * surfaceGroup = "Surface and upper air";
        const char * fireGroup = "Fire weather";
        const char * otherGroup = "Other fields";

        const vector<Curated>& curated() {
            static const vector<Curated> list{
                {"cref", "Composite Reflectivity (one member)", severeGroup},
                {"cref_max", "Composite Reflectivity (ensemble maximum)", severeGroup},
                {"cref_pb_t040", "Composite Reflectivity Paintball > 40 dBZ", severeGroup},
                {"cref_nmep_r013_t040", "Composite Reflectivity > 40 dBZ, 40-km neighborhood probability", severeGroup},
                {"maxref1km_04h_max_overlap_max", "4-h Max Reflectivity (ensemble maximum)", severeGroup},
                {"maxref1km_mucape_04h_max_overlap_pb_t>040", "4-h Max Reflectivity Paintball > 40 dBZ (where MUCAPE > 50 J/kg)", severeGroup},
                {"maxref1km_mucape_04h_max_overlap_nmep_r013_t>040", "4-h Max Reflectivity > 40 dBZ, neighborhood probability", severeGroup},
                {"maxref1km_24h_max_overlap12h_max", "24-h Max Reflectivity (ensemble maximum)", severeGroup},
                {"maxref1km_mucape_24h_max_overlap12h_pb_t>040", "24-h Max Reflectivity Paintball > 40 dBZ (where MUCAPE > 50 J/kg)", severeGroup},
                {"uh25_04h_max_overlap_max", "4-h Max 2-5 km Updraft Helicity (ensemble maximum)", severeGroup},
                {"uh25_04h_max_overlap_pb_t>99.85p", "4-h Paintball 2-5 km UH > 99.85 percentile", severeGroup},
                {"uh25_04h_max_overlap_pb_t>99.95p", "4-h Paintball 2-5 km UH > 99.95 percentile", severeGroup},
                {"uh25_04h_max_overlap_nmep_r013_t>99.85p", "4-h 2-5 km UH > 99.85 percentile, 40-km neighborhood probability", severeGroup},
                {"uh25_04h_max_overlap_nmep_r013_t>99.95p", "4-h 2-5 km UH > 99.95 percentile, 40-km neighborhood probability", severeGroup},
                {"uh25_04h_max_overlap", "4-h Max 2-5 km Updraft Helicity (one member)", severeGroup},
                {"uh03_04h_max_overlap", "4-h Max 0-3 km Updraft Helicity (one member)", severeGroup},
                {"uh25_24h_max_overlap12h_max", "24-h Max 2-5 km Updraft Helicity (ensemble maximum)", severeGroup},
                {"uh25_24h_max_overlap12h_pb_t>99.85p", "24-h Paintball 2-5 km UH > 99.85 percentile", severeGroup},
                {"uh25_24h_max_overlap12h_pb_t>99.95p", "24-h Paintball 2-5 km UH > 99.95 percentile", severeGroup},
                {"uh25_01h_max_max", "1-h Max 2-5 km Updraft Helicity (ensemble maximum)", severeGroup},
                {"uh25min_04h_min_overlap_min", "4-h Min 2-5 km Updraft Helicity (ensemble minimum, shown as magnitude)", severeGroup},
                {"uh25min_04h_min_overlap", "4-h Min 2-5 km Updraft Helicity (one member, shown as magnitude)", severeGroup},
                {"uh25min_24h_min_overlap12h_min", "24-h Min 2-5 km Updraft Helicity (ensemble minimum, shown as magnitude)", severeGroup},
                {"wmax_04h_max_overlap_max", "4-h Max Updraft Speed (ensemble maximum)", severeGroup},
                {"wmax_04h_max_overlap_pb_t020", "4-h Paintball Updraft Speed > 20 m/s", severeGroup},
                {"wmax_04h_max_overlap_nmep_r013_t020", "4-h Updraft Speed > 20 m/s, neighborhood probability", severeGroup},
                {"wmax_04h_max_overlap_nmep_r013_t030", "4-h Updraft Speed > 30 m/s, neighborhood probability", severeGroup},
                {"wmax_24h_max_overlap12h_max", "24-h Max Updraft Speed (ensemble maximum)", severeGroup},
                {"wmax_24h_max_overlap12h_pb_t020", "24-h Paintball Updraft Speed > 20 m/s", severeGroup},
                {"s10m_04h_max_overlap_max", "4-h Max 10-m Wind Speed where Reflectivity > 20 dBZ (ensemble maximum)", severeGroup},
                {"s10m_04h_max_overlap_pb_t030", "4-h Paintball 10-m Wind Speed > 30 kt", severeGroup},
                {"s10m_04h_max_overlap_nmep_r013_t030", "4-h 10-m Wind Speed > 30 kt, neighborhood probability", severeGroup},
                {"s10m_04h_max_overlap_nmep_r013_t050", "4-h 10-m Wind Speed > 50 kt, neighborhood probability", severeGroup},
                {"s10m_24h_max_overlap12h_max", "24-h Max 10-m Wind Speed where Reflectivity > 20 dBZ (ensemble maximum)", severeGroup},
                {"s10m_24h_max_overlap12h_pb_t030", "24-h Paintball 10-m Wind Speed > 30 kt", severeGroup},
                {"sbcape_mean", "Surface-Based CAPE (ensemble mean)", instabilityGroup},
                {"sbcape_max", "Surface-Based CAPE (ensemble maximum)", instabilityGroup},
                {"sbcape_min", "Surface-Based CAPE (ensemble minimum)", instabilityGroup},
                {"sbcape_prob_t>500", "SBCAPE > 500 J/kg (ensemble probability)", instabilityGroup},
                {"sbcape_prob_t>1000", "SBCAPE > 1000 J/kg (ensemble probability)", instabilityGroup},
                {"sbcape_prob_t>2000", "SBCAPE > 2000 J/kg (ensemble probability)", instabilityGroup},
                {"mucape_mean", "Most-Unstable CAPE (ensemble mean)", instabilityGroup},
                {"sbcinh_mean", "Surface-Based CIN (ensemble mean, shown as magnitude)", instabilityGroup},
                {"srh01_mean", "0-1 km Storm-Relative Helicity (ensemble mean)", instabilityGroup},
                {"srh03_mean", "0-3 km Storm-Relative Helicity (ensemble mean)", instabilityGroup},
                {"shrmag06_mean", "0-6 km Shear Magnitude (ensemble mean)", instabilityGroup},
                {"stp_fixed_mean", "Fixed-Layer Significant Tornado Parameter (ensemble mean)", instabilityGroup},
                {"stp_fixed_prob_t>001", "Fixed-Layer STP > 1 (ensemble probability)", instabilityGroup},
                {"stp_fixed_prob_t>003", "Fixed-Layer STP > 3 (ensemble probability)", instabilityGroup},
                {"qpf_01h_acc_mean", "1-h QPF (ensemble mean)", precipitationGroup},
                {"qpf_01h_acc_prob_t>0p010", "1-h QPF > 0.01 in (ensemble probability)", precipitationGroup},
                {"qpf_01h_acc_prob_t>001", "1-h QPF > 1 in (ensemble probability)", precipitationGroup},
                {"qpf_03h_acc_pmmean", "3-h QPF (PM mean)", precipitationGroup},
                {"qpf_03h_acc_mean", "3-h QPF (ensemble mean)", precipitationGroup},
                {"qpf_03h_acc_max", "3-h QPF (ensemble maximum)", precipitationGroup},
                {"qpf_03h_acc_nmep_r013_t>001", "3-h QPF > 1 in, neighborhood probability", precipitationGroup},
                {"qpf_03h_acc_nmep_r013_t>003", "3-h QPF > 3 in, neighborhood probability", precipitationGroup},
                {"qpf_06h_acc_pmmean", "6-h QPF (PM mean)", precipitationGroup},
                {"qpf_06h_acc_mean", "6-h QPF (ensemble mean)", precipitationGroup},
                {"qpf_06h_acc_max", "6-h QPF (ensemble maximum)", precipitationGroup},
                {"qpf_06h_acc", "6-h QPF (one member)", precipitationGroup},
                {"qpf_06h_acc_prob_t>0p010", "6-h QPF > 0.01 in (ensemble probability)", precipitationGroup},
                {"qpf_06h_acc_prob_t>001", "6-h QPF > 1 in (ensemble probability)", precipitationGroup},
                {"qpf_06h_acc_prob_t>002", "6-h QPF > 2 in (ensemble probability)", precipitationGroup},
                {"qpf_06h_acc_prob_t>003", "6-h QPF > 3 in (ensemble probability)", precipitationGroup},
                {"qpf_06h_acc_nmep_r013_t>0p100", "6-h QPF > 0.1 in, neighborhood probability", precipitationGroup},
                {"qpf_06h_acc_nmep_r013_t>0p250", "6-h QPF > 0.25 in, neighborhood probability", precipitationGroup},
                {"qpf_24h_acc_overlap12h_pmmean", "24-h QPF (PM mean)", precipitationGroup},
                {"qpf_24h_acc_overlap12h_mean", "24-h QPF (ensemble mean)", precipitationGroup},
                {"qpf_24h_acc_overlap12h_max", "24-h QPF (ensemble maximum)", precipitationGroup},
                {"qpf_48h_acc_overlap12h_pmmean", "48-h QPF (PM mean)", precipitationGroup},
                {"qpf_48h_acc_overlap12h_mean", "48-h QPF (ensemble mean)", precipitationGroup},
                {"qpf_48h_acc_overlap12h_max", "48-h QPF (ensemble maximum)", precipitationGroup},
                {"snow_01h_acc_mean", "1-h Snowfall (ensemble mean)", precipitationGroup},
                {"snow_01h_acc_prob_t>001", "1-h Snowfall > 1 in (ensemble probability)", precipitationGroup},
                {"snow_01h_acc_prob_t>002", "1-h Snowfall > 2 in (ensemble probability)", precipitationGroup},
                {"snow_12h_acc_pmmean", "12-h Snowfall (PM mean)", precipitationGroup},
                {"snow_12h_acc_mean", "12-h Snowfall (ensemble mean)", precipitationGroup},
                {"snow_12h_acc_max", "12-h Snowfall (ensemble maximum)", precipitationGroup},
                {"snow_12h_acc_prob_t>004", "12-h Snowfall > 4 in (ensemble probability)", precipitationGroup},
                {"snow_12h_acc_prob_t>008", "12-h Snowfall > 8 in (ensemble probability)", precipitationGroup},
                {"snow_12h_acc_prob_t>012", "12-h Snowfall > 12 in (ensemble probability)", precipitationGroup},
                {"snow_24h_acc_overlap12h_pmmean", "24-h Snowfall (PM mean)", precipitationGroup},
                {"snow_24h_acc_overlap12h_mean", "24-h Snowfall (ensemble mean)", precipitationGroup},
                {"snow_24h_acc_overlap12h_max", "24-h Snowfall (ensemble maximum)", precipitationGroup},
                {"frzr_qpf_03h_acc_mean", "3-h Freezing Rain QPF (ensemble mean)", precipitationGroup},
                {"frzr_qpf_03h_acc_max", "3-h Freezing Rain QPF (ensemble maximum)", precipitationGroup},
                {"frzr_qpf_03h_acc_prob_t>0p060", "3-h Freezing Rain QPF > 0.06 in (ensemble probability)", precipitationGroup},
                {"frzr_fram_03h_acc_mean", "3-h FRAM Ice Accretion (ensemble mean)", precipitationGroup},
                {"frzr_qpf_24h_acc_overlap12h_mean", "24-h Freezing Rain QPF (ensemble mean)", precipitationGroup},
                {"frzr_qpf_24h_acc_overlap12h_max", "24-h Freezing Rain QPF (ensemble maximum)", precipitationGroup},
                {"frzr_fram_24h_acc_overlap12h_mean", "24-h FRAM Ice Accretion (ensemble mean)", precipitationGroup},
                {"t2m_mean", "2-m Temperature (ensemble mean)", surfaceGroup},
                {"td2m_mean", "2-m Dewpoint (ensemble mean)", surfaceGroup},
                {"pmsl_mean", "Mean Sea-Level Pressure (ensemble mean)", surfaceGroup},
                {"pwat_mean", "Precipitable Water (ensemble mean)", surfaceGroup},
                {"pwat_prob_t>0p500_t<0p800", "Precipitable Water between 0.5 and 0.8 in (ensemble probability)", surfaceGroup},
                {"cld_low_mean", "Low Cloud Cover (ensemble mean)", surfaceGroup},
                {"cld_mid_mean", "Mid Cloud Cover (ensemble mean)", surfaceGroup},
                {"cld_high_mean", "High Cloud Cover (ensemble mean)", surfaceGroup},
                {"z500_mean", "500 mb Height (ensemble mean)", surfaceGroup},
                {"z700_mean", "700 mb Height (ensemble mean)", surfaceGroup},
                {"z850_mean", "850 mb Height (ensemble mean)", surfaceGroup},
                {"t500_mean", "500 mb Temperature (ensemble mean)", surfaceGroup},
                {"t700_mean", "700 mb Temperature (ensemble mean)", surfaceGroup},
                {"t850_mean", "850 mb Temperature (ensemble mean)", surfaceGroup},
                {"gust_mean", "10-m Gusts (ensemble mean)", surfaceGroup},
                {"gust_max", "10-m Gusts (ensemble maximum)", surfaceGroup},
                {"gust_min", "10-m Gusts (ensemble minimum)", surfaceGroup},
                {"s10m_01h_max_mean", "1-h Max 10-m Wind (ensemble mean)", surfaceGroup},
                {"s10m_01h_max_max", "1-h Max 10-m Wind (ensemble maximum)", surfaceGroup},
                {"s10m_01h_max_min", "1-h Max 10-m Wind (ensemble minimum)", surfaceGroup},
                {"rh2m_mean", "2-m Relative Humidity (ensemble mean)", fireGroup},
                {"rh2m_max", "2-m Relative Humidity (ensemble maximum)", fireGroup},
                {"rh2m_min", "2-m Relative Humidity (ensemble minimum)", fireGroup},
                {"fosberg_mean", "Fosberg Index (ensemble mean)", fireGroup},
                {"fosberg_max", "Fosberg Index (ensemble maximum)", fireGroup},
                {"fosberg_min", "Fosberg Index (ensemble minimum)", fireGroup},
                {"fosberg_prob_t>050", "Fosberg Index > 50 (ensemble probability)", fireGroup},
                {"fosberg_prob_t>075", "Fosberg Index > 75 (ensemble probability)", fireGroup},
                {"rh2m_s10m_prob_t<020_t>015", "Wind > 15 mph and RH < 20% (ensemble probability)", fireGroup},
            };
            return list;
        }

        const std::map<string, unsigned>& memberColours() {   // the colours SPC's viewer uses for each member's paintball
            static const std::map<string, unsigned> colours{
                {"HRRR", 0x4c4c4c}, {"RRFS", 0x007399}, {"REFS M01", 0x009985}, {"REFS M02", 0x009947}, {"REFS M03", 0x009909},
                {"REFS M05", 0x469900}, {"HRRR TL06", 0x7f7f7f}, {"RRFS TL06", 0x4cd3ff}, {"HRRR TL12", 0xb3b3b3}, {"RRFS TL12", 0xb3ecff}};
            return colours;
        }

        bool contains(const string& text, const char * part) { return text.find(part) != string::npos; }
        bool startsWith(const string& text, const char * part) { return text.rfind(part, 0) == 0; }

        // colours, units and shown-value conversion from the array's name and units
        void applyScale(Product& p, const string& units) {
            const auto& id = p.id;
            if (contains(id, "_pb_")) {
                p.kind = Kind::Paintball;
                p.units = "members";
                return;
            }
            if (contains(id, "nmep") || contains(id, "_prob")) {
                p.kind = Kind::Banded;
                p.stops = probabilityStops();
                p.factor = 100.0;
                p.units = "%";
                p.hideBelow = 5.0;
                p.digits = 0;
                return;
            }
            p.kind = Kind::Banded;
            if (units == "dBz") {
                p.stops = reflectivityStops();
                p.units = "dBZ";
                p.hideBelow = 10.0;
            } else if (units == "meter ** 2 / second ** 2") {
                if (contains(id, "srh")) {
                    p.stops = helicityStops();
                    p.hideBelow = 50.0;
                } else {
                    p.stops = updraftHelicityStops();
                    p.hideBelow = 25.0;
                    if (contains(id, "uh25min")) {
                        p.factor = -1.0;   // negative (anticyclonic) helicity shown as a magnitude
                    }
                }
                p.units = "m2/s2";
                p.digits = 0;
            } else if (units == "inch") {
                if (startsWith(id, "snow")) {
                    p.stops = snowStops();
                    p.hideBelow = 0.1;
                } else if (startsWith(id, "frzr_fram")) {
                    p.stops = iceAccretionStops();
                    p.hideBelow = 0.01;
                } else if (startsWith(id, "frzr")) {
                    p.stops = freezingRainStops();
                    p.hideBelow = 0.01;
                } else if (startsWith(id, "pwat")) {
                    p.stops = precipitableWaterStops();
                    p.hideBelow = 0.0;
                } else {
                    p.stops = qpfStops();
                    p.hideBelow = 0.01;
                }
                p.units = "in";
                p.digits = 2;
            } else if (units == "percent") {
                if (startsWith(id, "cld_")) {
                    p.kind = Kind::Blended;
                    p.stops = cloudStops(contains(id, "low") ? 0x19199e : (contains(id, "mid") ? 0x3c7d2b : 0x8c2721));
                    p.hideBelow = 10.0;
                } else {
                    p.stops = relativeHumidityStops();
                }
                p.units = "%";
                p.digits = 0;
            } else if (units == "degree_Fahrenheit" || units == "degree_Celsius") {
                p.kind = Kind::Blended;
                p.stops = temperatureStops(units == "degree_Fahrenheit");
                p.units = units == "degree_Fahrenheit" ? "F" : "C";
            } else if (units == "knot") {
                if (id.size() > 1 && (id[0] == 'u' || id[0] == 'v') && (contains(id, "10m_mean") || contains(id, "_mean") || contains(id, "shr"))) {
                    p.kind = Kind::Blended;
                    p.stops = componentStops();
                } else {
                    p.stops = windSpeedStops(1.0);
                    p.hideBelow = 10.0;
                }
                p.units = "kt";
            } else if (units == "mile / hour") {
                p.stops = windSpeedStops(1.15);
                p.units = "mph";
                p.hideBelow = 11.5;
            } else if (units == "meter / second") {
                p.stops = updraftSpeedStops();
                p.units = "m/s";
                p.hideBelow = 10.0;
            } else if (units == "joule / kilogram") {
                if (contains(id, "cinh")) {
                    p.stops = cinStops();
                    p.factor = -1.0;
                    p.hideBelow = 25.0;
                } else {
                    p.stops = capeStops();
                    p.hideBelow = 100.0;
                }
                p.units = "J/kg";
                p.digits = 0;
            } else if (units == "meter") {
                p.kind = Kind::Blended;
                if (contains(id, "z500")) {
                    p.stops = rainbow(5000, 6000);
                } else if (contains(id, "z700")) {
                    p.stops = rainbow(2700, 3300);
                } else {
                    p.stops = rainbow(1200, 1650);
                }
                p.units = "m";
                p.digits = 0;
            } else if (units == "pascal") {
                p.kind = Kind::Blended;
                p.factor = 0.01;
                p.stops = rainbow(985, 1035);
                p.units = "hPa";
            } else if (units == "dimensionless") {
                if (contains(id, "stp")) {
                    p.stops = stpStops();
                    p.hideBelow = 0.5;
                } else {
                    p.kind = Kind::Blended;
                    p.stops = rainbow(0, 70);
                }
                p.units = "";
            } else if (units == "degree_Celsius") {
                p.kind = Kind::Blended;
                p.stops = temperatureStops(false);
                p.units = "C";
            } else {
                p.kind = Kind::Blended;   // no table: the colours span each picture's own range
                p.units = units;
            }
        }

        string tidy(const string& id) {
            string text = id;
            std::replace(text.begin(), text.end(), '_', ' ');
            return text;
        }
    }

    string memberDisplayName(const string& raw) {
        if (raw == "RRFS") {
            return "RRFS Ctl";
        }
        if (raw == "HRRR TL06") {
            return "HRRR -6h";
        }
        if (raw == "HRRR TL12") {
            return "HRRR -12h";
        }
        if (raw == "RRFS TL06") {
            return "RRFS Ctl -6h";
        }
        if (raw == "RRFS TL12") {
            return "RRFS Ctl -12h";
        }
        return raw;
    }

    Product describe(const string& id, const ZarrStore::Array& array) {
        Product p;
        p.id = id;
        p.label = tidy(id);
        p.group = otherGroup;
        for (const auto& c : curated()) {
            if (id == c.id) {
                p.label = c.title;
                p.group = c.group;
                break;
            }
        }
        p.members = array.dims.size() == 4;
        if (array.dims.size() >= 3) {
            p.timeDimension = array.dims[array.dims.size() - 3];
        }
        applyScale(p, array.attrs.value("units").toString().toStdString());
        return p;
    }

    vector<Product> catalog(const ZarrStore& store) {
        vector<Product> out;
        for (const auto& id : store.arrayNames()) {
            const auto * a = store.array(id);
            if (a == nullptr || a->shape.size() < 3 || a->shape[a->shape.size() - 1] != LambertGrid::columns ||
                a->shape[a->shape.size() - 2] != LambertGrid::rows || id == "ptype_consensus") {   // (the precipitation-type codes have no documented key)
                continue;
            }
            out.push_back(describe(id, *a));
        }
        static const vector<string> groupOrder{severeGroup, instabilityGroup, precipitationGroup, surfaceGroup, fireGroup, otherGroup};
        const auto position = [] (const Product& p) {
            size_t i = 0;
            for (; i < curated().size(); i += 1) {
                if (p.id == curated()[i].id) {
                    break;
                }
            }
            return i;
        };
        std::stable_sort(out.begin(), out.end(), [&] (const Product& a, const Product& b) {
            const auto ga = std::find(groupOrder.begin(), groupOrder.end(), a.group) - groupOrder.begin();
            const auto gb = std::find(groupOrder.begin(), groupOrder.end(), b.group) - groupOrder.begin();
            if (ga != gb) {
                return ga < gb;
            }
            const auto pa = position(a);
            const auto pb = position(b);
            return pa != pb ? pa < pb : a.id < b.id;
        });
        return out;
    }

    string cycleUrl(const QDateTime& initUtc) {
        const auto utc = initUtc.toUTC();
        return "https://www.spc.noaa.gov/exper/refs/data/" + utc.toString("yyyy/MM/dd").toStdString() + "/refs-spc_" +
            utc.toString("yyyyMMdd_HHmm").toStdString() + ".zarr";
    }

    vector<QDateTime> findCycles(int wanted) {
        vector<QDateTime> found;
        auto when = QDateTime::currentDateTimeUtc();
        const int hour = (when.time().hour() / 6) * 6;
        when.setTime(QTime{hour, 0});
        for (int probe = 0; probe < 20 && static_cast<int>(found.size()) < wanted; probe += 1, when = when.addSecs(-6 * 3600)) {
            int status = 0;
            const auto bytes = URL::getBytesWithStatus(cycleUrl(when) + "/.zgroup", status);
            if (status == 200 && !bytes.isEmpty()) {
                found.push_back(when);
            }
        }
        return found;
    }

    double shownValue(const Product& p, const vector<float>& values, int column, int row) {
        if (column < 0 || column >= LambertGrid::columns || row < 0 || row >= LambertGrid::rows) {
            return std::nan("");
        }
        const float v = values[static_cast<size_t>(row) * LambertGrid::columns + column];
        if (std::isnan(v)) {
            return std::nan("");
        }
        return v * p.factor + p.offset;
    }

    namespace {
        QRgb colourFor(const Product& p, double shown, double lo, double hi) {
            if (std::isnan(shown) || shown < p.hideBelow) {
                return 0;
            }
            const auto& stops = p.stops;
            if (stops.empty()) {
                return 0;
            }
            if (p.kind == Kind::Banded) {
                if (shown < stops.front().value - 1e-9) {
                    return 0;
                }
                size_t band = 0;
                while (band + 1 < stops.size() && shown >= stops[band + 1].value - 1e-9) {
                    band += 1;
                }
                return qRgba(stops[band].r, stops[band].g, stops[band].b, 255);
            }
            // blended
            if (shown <= stops.front().value) {
                return qRgba(stops.front().r, stops.front().g, stops.front().b, 255);
            }
            for (size_t i = 1; i < stops.size(); i += 1) {
                if (shown <= stops[i].value) {
                    const double t = (shown - stops[i - 1].value) / (stops[i].value - stops[i - 1].value);
                    return qRgba(static_cast<int>(stops[i - 1].r + t * (stops[i].r - stops[i - 1].r)),
                                 static_cast<int>(stops[i - 1].g + t * (stops[i].g - stops[i - 1].g)),
                                 static_cast<int>(stops[i - 1].b + t * (stops[i].b - stops[i - 1].b)), 255);
                }
            }
            (void)lo;
            (void)hi;
            return qRgba(stops.back().r, stops.back().g, stops.back().b, 255);
        }

        // a product with no colour table gets one spanning the picture's own range
        Product withAutoStops(const Product& p, const vector<float>& values) {
            if (!p.stops.empty() || p.kind == Kind::Paintball) {
                return p;
            }
            double lo = 1e30;
            double hi = -1e30;
            for (const float v : values) {
                if (!std::isnan(v)) {
                    const double shown = v * p.factor + p.offset;
                    lo = std::min(lo, shown);
                    hi = std::max(hi, shown);
                }
            }
            Product out = p;
            out.kind = Kind::Blended;
            out.stops = rainbow(lo, hi > lo ? hi : lo + 1.0);
            return out;
        }
    }

    View viewForBox(const string& name, double west, double south, double east, double north, int pixelsPerCell) {
        static const LambertGrid grid;
        double minCol = 1e9, maxCol = -1e9, minRow = 1e9, maxRow = -1e9;
        // the box is a rectangle in latitude / longitude, which bends on the Lambert grid: walk its outline
        for (int i = 0; i <= 40; i += 1) {
            const double t = i / 40.0;
            const double lon = west + (east - west) * t;
            const double lat = south + (north - south) * t;
            for (const auto& [la, lo] : {std::pair<double, double>{south, lon}, {north, lon}, {lat, west}, {lat, east}}) {
                double c, r;
                grid.toGrid(la, lo, c, r);
                minCol = std::min(minCol, c);
                maxCol = std::max(maxCol, c);
                minRow = std::min(minRow, r);
                maxRow = std::max(maxRow, r);
            }
        }
        View v;
        v.name = name;
        v.col0 = std::clamp(static_cast<int>(std::floor(minCol)), 0, LambertGrid::columns - 2);
        v.row0 = std::clamp(static_cast<int>(std::floor(minRow)), 0, LambertGrid::rows - 2);
        const int col1 = std::clamp(static_cast<int>(std::ceil(maxCol)), v.col0 + 1, LambertGrid::columns - 1);
        const int row1 = std::clamp(static_cast<int>(std::ceil(maxRow)), v.row0 + 1, LambertGrid::rows - 1);
        v.cols = col1 - v.col0 + 1;
        v.rows = row1 - v.row0 + 1;
        v.scale = std::clamp(pixelsPerCell, 1, 8);
        return v;
    }

    void cellAt(const View& view, double fx, double fy, int& column, int& row) {
        column = view.col0 + static_cast<int>(std::floor(fx * view.cols));
        row = view.row0 + view.rows - 1 - static_cast<int>(std::floor(fy * view.rows));
    }

    namespace {
        float sampleAt(const vector<float>& values, double column, double row, bool smooth) {
            const int w = LambertGrid::columns;
            const int h = LambertGrid::rows;
            if (!smooth) {
                const int c = std::clamp(static_cast<int>(std::lround(column)), 0, w - 1);
                const int r = std::clamp(static_cast<int>(std::lround(row)), 0, h - 1);
                return values[static_cast<size_t>(r) * w + c];
            }
            const double cc = std::clamp(column, 0.0, w - 1.0);
            const double rr = std::clamp(row, 0.0, h - 1.0);
            const int c0 = std::min(static_cast<int>(cc), w - 2);
            const int r0 = std::min(static_cast<int>(rr), h - 2);
            const double fx = cc - c0;
            const double fy = rr - r0;
            const float a = values[static_cast<size_t>(r0) * w + c0];
            const float b = values[static_cast<size_t>(r0) * w + c0 + 1];
            const float c = values[static_cast<size_t>(r0 + 1) * w + c0];
            const float d = values[static_cast<size_t>(r0 + 1) * w + c0 + 1];
            if (std::isnan(a) || std::isnan(b) || std::isnan(c) || std::isnan(d)) {
                return fx < 0.5 ? (fy < 0.5 ? a : c) : (fy < 0.5 ? b : d);
            }
            return static_cast<float>((a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy);
        }
    }

    QImage dataImage(const Product& product, const vector<float>& values, const vector<string>& memberNames, const View& view) {
        const int outW = view.cols * view.scale;
        const int outH = view.rows * view.scale;
        QImage image{outW, outH, QImage::Format_ARGB32};
        image.fill(0);
        if (values.size() < static_cast<size_t>(LambertGrid::columns) * LambertGrid::rows) {
            return image;
        }
        const bool paintball = product.kind == Kind::Paintball;
        vector<QRgb> colours;
        if (paintball) {
            // each member that is over the threshold paints its colour over the ones before it (SPC's "paintball")
            for (size_t i = 0; i < 10; i += 1) {
                const string name = i < memberNames.size() ? memberNames[i] : string{};
                const auto found = memberColours().find(name);
                colours.push_back(found == memberColours().end() ? 0x808080u : found->second);
            }
        }
        const auto p = paintball ? product : withAutoStops(product, values);
        for (int y = 0; y < outH; y += 1) {
            auto * out = reinterpret_cast<QRgb *>(image.scanLine(y));
            const double row = view.row0 + view.rows - (y + 0.5) / view.scale - 0.5;
            for (int x = 0; x < outW; x += 1) {
                const double column = view.col0 + (x + 0.5) / view.scale - 0.5;
                if (paintball) {
                    const float raw = sampleAt(values, column, row, false);
                    const int mask = std::isnan(raw) ? 0 : static_cast<int>(raw);
                    if (mask == 0) {
                        continue;
                    }
                    double r = 255;
                    double g = 255;
                    double b = 255;
                    for (int bit = 0; bit < 10; bit += 1) {
                        if (mask & (1 << bit)) {
                            const double alpha = 0.55;
                            r = r * (1 - alpha) + ((colours[static_cast<size_t>(bit)] >> 16) & 255) * alpha;
                            g = g * (1 - alpha) + ((colours[static_cast<size_t>(bit)] >> 8) & 255) * alpha;
                            b = b * (1 - alpha) + (colours[static_cast<size_t>(bit)] & 255) * alpha;
                        }
                    }
                    out[x] = qRgba(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), 255);
                } else {
                    const float raw = sampleAt(values, column, row, view.scale > 1);
                    out[x] = colourFor(p, std::isnan(raw) ? std::nan("") : raw * p.factor + p.offset, 0, 0);
                }
            }
        }
        return image;
    }

    namespace {
        // line segments of the radar screens' geometry files (latitude, west-positive longitude pairs)
        const vector<float>& geometry(RadarGeometryTypeEnum type) {
            static std::mutex mutex;
            static std::map<RadarGeometryTypeEnum, vector<float>> loaded;
            std::lock_guard<std::mutex> lock{mutex};
            auto& data = loaded[type];
            if (data.empty()) {
                RadarGeomInfo::loadData(RadarGeomInfo::typeToFileName.at(type), data);
            }
            return data;
        }

        void drawLines(QPainter& painter, RadarGeometryTypeEnum type, const QColor& colour, double width, const LambertGrid& grid, const View& view) {
            const auto& data = geometry(type);
            painter.setPen(QPen{colour, width});
            QVector<QLineF> lines;
            for (size_t i = 0; i + 3 < data.size(); i += 4) {
                double c1, r1, c2, r2;
                grid.toGrid(data[i], -data[i + 1], c1, r1);
                grid.toGrid(data[i + 2], -data[i + 3], c2, r2);
                const double left = view.col0 - 1.0;
                const double right = view.col0 + view.cols;
                const double bottom = view.row0 - 1.0;
                const double top = view.row0 + view.rows;
                if ((c1 < left && c2 < left) || (c1 > right && c2 > right) || (r1 < bottom && r2 < bottom) || (r1 > top && r2 > top)) {
                    continue;
                }
                const auto px = [&] (double c) { return (c - view.col0 + 0.5) * view.scale; };
                const auto py = [&] (double r) { return (view.row0 + view.rows - r - 0.5) * view.scale; };
                lines.push_back({px(c1), py(r1), px(c2), py(r2)});
            }
            painter.drawLines(lines);
        }

        void drawKey(QPainter& painter, const Product& p, const vector<string>& memberNames, int w, int h) {
            QFont font{painter.font()};
            font.setPixelSize(std::min(24, 15 + (w / 1000)));
            painter.setFont(font);
            const QFontMetrics metrics{font};
            const double y0 = h - 44 - (font.pixelSize() - 15) * 2;
            if (p.kind == Kind::Paintball) {
                double x = 10;
                painter.fillRect(QRectF{4, y0 - 6, w - 8.0, 46}, QColor{255, 255, 255, 215});
                for (size_t i = 0; i < 10 && i < memberNames.size(); i += 1) {
                    const auto name = memberDisplayName(memberNames[i]);
                    const auto found = memberColours().find(memberNames[i]);
                    const QColor colour{static_cast<QRgb>(found == memberColours().end() ? 0x808080u : found->second)};
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(colour);
                    painter.drawRect(QRectF{x, y0, 18, 18});
                    painter.setPen(Qt::black);
                    painter.drawText(QPointF{x + 23, y0 + 14}, QString::fromStdString(name));
                    x += 23 + metrics.horizontalAdvance(QString::fromStdString(name)) + 22;
                }
                return;
            }
            const auto& stops = p.stops;
            if (stops.empty()) {
                return;
            }
            const double boxWidth = std::min(60.0, (w - 140.0) / static_cast<double>(stops.size()));
            painter.fillRect(QRectF{4, y0 - 6, 10 + boxWidth * stops.size() + 20 + metrics.horizontalAdvance(QString::fromStdString(p.units)) + 20, 46}, QColor{255, 255, 255, 215});
            for (size_t i = 0; i < stops.size(); i += 1) {
                const QRectF box{10 + boxWidth * static_cast<double>(i), y0, boxWidth, 16};
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor{stops[i].r, stops[i].g, stops[i].b});
                painter.drawRect(box);
                painter.setPen(Qt::black);
                painter.drawText(QRectF{box.left() - 12, y0 + 17, boxWidth + 24, 18}, Qt::AlignCenter, QString::number(stops[i].value, 'g', 3));
            }
            painter.setPen(Qt::black);
            painter.drawText(QPointF{10 + boxWidth * static_cast<double>(stops.size()) + 8, y0 + 13}, QString::fromStdString(p.units));
        }
    }

    QByteArray renderPng(const Product& product, const vector<float>& values, const vector<string>& memberNames, const View& view) {
        const int w = view.cols * view.scale;
        const int h = view.rows * view.scale;
        static const LambertGrid grid;
        QImage image{w, h, QImage::Format_ARGB32_Premultiplied};
        image.fill(QColor{244, 244, 240});
        const auto withStops = withAutoStops(product, values);
        const double line = std::max(1.0, view.scale * 0.45);   // line weights grow with the picture
        {
            QPainter painter{&image};
            painter.setRenderHint(QPainter::Antialiasing, true);
            drawLines(painter, LakeLines, QColor{190, 205, 225}, 0.8 * line, grid, view);
            drawLines(painter, CountyLines, QColor{205, 205, 200, 150}, 0.5 * line, grid, view);
            painter.drawImage(0, 0, dataImage(withStops, values, memberNames, view));
            drawLines(painter, CaLines, QColor{90, 90, 90}, 1.0 * line, grid, view);
            drawLines(painter, MxLines, QColor{90, 90, 90}, 1.0 * line, grid, view);
            drawLines(painter, StateLines, QColor{50, 50, 50}, 1.3 * line, grid, view);
            drawKey(painter, withStops, memberNames, w, h);
        }
        QByteArray png;
        QBuffer buffer{&png};
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return png;
    }

    string readout(const Product& p, const vector<float>& values, int column, int row, const vector<string>& memberNames) {
        if (column < 0 || column >= LambertGrid::columns || row < 0 || row >= LambertGrid::rows ||
            values.size() < static_cast<size_t>(LambertGrid::columns) * LambertGrid::rows) {
            return {};
        }
        static const LambertGrid grid;
        double lat = 0.0;
        double lon = 0.0;
        grid.toLatLon(column + 0.5, row + 0.5, lat, lon);
        string text = QString{"%1%2 %3%4   "}.arg(std::fabs(lat), 0, 'f', 2).arg(lat >= 0 ? "N" : "S").arg(std::fabs(lon), 0, 'f', 2).arg(lon < 0 ? "W" : "E").toStdString();
        const float raw = values[static_cast<size_t>(row) * LambertGrid::columns + column];
        if (std::isnan(raw)) {
            return text + "no data";
        }
        if (p.kind == Kind::Paintball) {
            const int mask = static_cast<int>(raw);
            int count = 0;
            string names;
            for (int bit = 0; bit < 10; bit += 1) {
                if (mask & (1 << bit)) {
                    count += 1;
                    if (static_cast<size_t>(bit) < memberNames.size()) {
                        names += (names.empty() ? "" : ", ") + memberDisplayName(memberNames[static_cast<size_t>(bit)]);
                    }
                }
            }
            return text + std::to_string(count) + " of 10 members" + (names.empty() ? "" : ": " + names);
        }
        return text + QString::number(raw * p.factor + p.offset, 'f', p.digits).toStdString() + (p.units.empty() ? "" : " " + p.units);
    }
}
