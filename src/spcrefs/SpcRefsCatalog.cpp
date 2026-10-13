// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// What SPC's REFS products are and how each is coloured (the curated list, the colour tables, the units): no network and no drawing here, so the chart registry can use it.

#include <algorithm>
#include <cmath>
#include <map>
#include "spcrefs/LambertGrid.h"
#include <mutex>
#include <QStringList>
#include "util/Utility.h"
#include "spcrefs/UtilitySpcRefs.h"

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

    namespace {
        std::mutex memberNamesMutex;
        vector<string> knownMemberNames;
    }

    void setMemberNames(const vector<string>& displayNames) {
        const std::lock_guard lock{memberNamesMutex};
        knownMemberNames = displayNames;
        QStringList joined;   // kept for the next start: a chart read from the disk cache never opens a store
        for (const auto& name : displayNames) {
            joined << QString::fromStdString(name);
        }
        Utility::writePref("SPCREFS_MEMBER_NAMES", joined.join('|').toStdString());
    }

    vector<string> memberNames() {
        const std::lock_guard lock{memberNamesMutex};
        if (knownMemberNames.empty()) {
            for (const auto& name : QString::fromStdString(Utility::readPref("SPCREFS_MEMBER_NAMES", "")).split('|', Qt::SkipEmptyParts)) {
                knownMemberNames.push_back(name.toStdString());
            }
        }
        return knownMemberNames;
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

    // where a product sits in SPC's menu order: its group, then its place in the curated list (unlisted ones last)
    std::pair<int, int> menuPosition(const Product& p) {
        static const vector<string> groupOrder{severeGroup, instabilityGroup, precipitationGroup, surfaceGroup, fireGroup, otherGroup};
        int place = 0;
        for (; place < static_cast<int>(curated().size()); place += 1) {
            if (p.id == curated()[static_cast<size_t>(place)].id) {
                break;
            }
        }
        return {static_cast<int>(std::find(groupOrder.begin(), groupOrder.end(), p.group) - groupOrder.begin()), place};
    }

    unsigned memberColour(const string& name) {
        const auto found = memberColours().find(name);
        return found == memberColours().end() ? 0x808080u : found->second;
    }

    vector<Stop> rainbowStops(double lo, double hi) {
        return rainbow(lo, hi);
    }

    // the units a product of the store is in, from its name (what the arrays' "units" attribute says; the registry has no store to read it from)
    static string unitsOf(const string& id) {
        const auto has = [&id] (const char * part) { return id.find(part) != string::npos; };
        const auto starts = [&id] (const char * part) { return id.rfind(part, 0) == 0; };
        if (has("_pb_") || has("nmep") || has("_prob")) {
            return "";
        }
        if (starts("cref") || starts("maxref")) return "dBz";
        if (starts("uh") || starts("srh")) return "meter ** 2 / second ** 2";
        if (starts("wmax")) return "meter / second";
        if (starts("s10m") || starts("shr") || starts("ushr") || starts("vshr") || starts("u10m") || starts("v10m")) return "knot";
        if (starts("gust")) return "mile / hour";
        if (starts("qpf") || starts("snow") || starts("frzr") || starts("pwat")) return "inch";
        if (starts("sbcape") || starts("mucape") || starts("sbcinh")) return "joule / kilogram";
        if (starts("stp") || starts("fosberg")) return "dimensionless";
        if (starts("t2m") || starts("td2m")) return "degree_Fahrenheit";
        if (starts("t500") || starts("t700") || starts("t850")) return "degree_Celsius";
        if (starts("z500") || starts("z700") || starts("z850")) return "meter";
        if (starts("pmsl")) return "pascal";
        if (starts("cld") || starts("rh2m")) return "percent";
        return "";
    }

    vector<Product> curatedProducts() {
        vector<Product> out;
        for (const auto& c : curated()) {
            ZarrStore::Array array;
            const bool member = string{c.title}.find("one member") != string::npos;
            array.dims = member ? vector<string>{"member", "valid_time", "y", "x"} : vector<string>{"valid_time", "y", "x"};
            array.attrs.insert("units", QString::fromStdString(unitsOf(c.id)));
            out.push_back(describe(c.id, array));
        }
        return out;
    }
}
