// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The High-Resolution Ensemble Forecast (HREF).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void hrefProducts(std::vector<Product>& p) {
    enum class Time { Instant, OneHour, ThreeHour, Total };
    enum class Look { Precip, Snow, Radar, Prob, Temp, Dew, Pwat, Cape, Vis, Shear, Plain };
    struct Row {
        const char * id;
        const char * label;
        const char * kind;
        const char * variable;
        const char * level;
        Time time;
        const char * detail;
        double scale;
        Look look;
        const char * title;
        bool pressure;
    };
    const char * const surface = "surface";
    const char * const column = "entire atmosphere (considered as a single layer)";
    const char * const ground = "1000 m above ground";
    const std::vector<Row> rows{
        {"mean_precip_p01", "Mean 1-hour Precipitation and MSLP", "mean", "APCP", surface, Time::OneHour, "", 1, Look::Precip, "Mean precipitation in the hour", true},
        {"mean_precip_p03", "Mean 3-hour Precipitation and MSLP", "mean", "APCP", surface, Time::ThreeHour, "", 1, Look::Precip, "Mean precipitation in 3 hours", true},
        {"mean_precip_ptot", "Mean Total Precipitation and MSLP", "mean", "APCP", surface, Time::Total, "", 1, Look::Precip, "Mean precipitation since the start of the run", true},
        {"blend_mean_precip_p01", "Blend Mean 1-hour Precipitation", "avrg", "APCP", surface, Time::OneHour, "", 1, Look::Precip, "Blended mean precipitation in the hour", false},
        {"blend_mean_precip_p03", "Blend Mean 3-hour Precipitation", "avrg", "APCP", surface, Time::ThreeHour, "", 1, Look::Precip, "Blended mean precipitation in 3 hours", false},
        {"blend_mean_precip_ptot", "Blend Mean Total Precipitation", "avrg", "APCP", surface, Time::Total, "", 1, Look::Precip, "Blended mean precipitation since the start of the run", false},
        {"lpmm_mean_precip_p01", "Localized Probability Matched Mean 1-hour Precipitation", "lpmm", "APCP", surface, Time::OneHour, "", 1, Look::Precip, "Localized probability-matched mean, the hour", false},
        {"lpmm_mean_precip_p03", "Localized Probability Matched Mean 3-hour Precipitation", "lpmm", "APCP", surface, Time::ThreeHour, "", 1, Look::Precip, "Localized probability-matched mean, 3 hours", false},
        {"lpmm_mean_precip_ptot", "Localized Probability Matched Mean Total Precipitation", "lpmm", "APCP", surface, Time::Total, "", 1, Look::Precip, "Localized probability-matched mean since the start of the run", false},
        {"pmm_refd_1km", "Probability Matched Mean of 1 km Reflectivity", "pmmn", "REFD", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the 1 km reflectivity (dBZ)", false},
        {"pmm_refd_1km_emsl", "Probability Matched Mean of 1 km Reflectivity and MSLP", "pmmn", "REFD", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the 1 km reflectivity (dBZ)", true},
        {"pmm_refd_max", "Probability Matched Mean of Maximum Reflectivity", "pmmn", "MAXREF", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the hourly maximum reflectivity (dBZ)", false},
        {"pmm_refd_max_emsl", "Probability Matched Mean of Maximum Reflectivity and MSLP", "pmmn", "MAXREF", ground, Time::Instant, "", 1, Look::Radar, "Probability-matched mean of the hourly maximum reflectivity (dBZ)", true},
        {"prob_refd_40dbz", "Probability of 1 km Reflectivity over 40 dBZ", "prob", "REFD", ground, Time::Instant, "prob >40", 1, Look::Prob, "Chance of 1 km reflectivity over 40 dBZ (%)", false},
        {"prob_refd_max_40dbz", "Probability of 1 km Maximum Reflectivity over 40 dBZ", "prob", "MAXREF", ground, Time::Instant, "prob >40", 1, Look::Prob, "Chance of the hourly maximum reflectivity over 40 dBZ (%)", false},
        {"prob_cref_40dbz", "Probability of Composite Reflectivity over 40 dBZ", "prob", "REFC", column, Time::Instant, "prob >40", 1, Look::Prob, "Chance of composite reflectivity over 40 dBZ (%)", false},
        {"prob_cref_50dbz", "Probability of Composite Reflectivity over 50 dBZ", "prob", "REFC", column, Time::Instant, "prob >50", 1, Look::Prob, "Chance of composite reflectivity over 50 dBZ (%)", false},
        {"prob_rain", "Probability of Rain", "prob", "CRAIN", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of rain (%)", false},
        {"prob_snow", "Probability of Snow", "prob", "CSNOW", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of snow (%)", false},
        {"prob_sleet", "Probability of Sleet", "prob", "CICEP", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of sleet (%)", false},
        {"prob_freezing_rain", "Probability of Freezing Rain", "prob", "CFRZR", surface, Time::Instant, "prob >=1 <0", 1, Look::Prob, "Chance of freezing rain (%)", false},
        {"prob_3h_rain_0.5in", "Probability of 3-hour Rain over 0.5 in", "prob", "APCP", surface, Time::ThreeHour, "prob >12.7", 1, Look::Prob, "Chance of more than 0.5 in of rain in 3 hours (%)", false},
        {"prob_3h_rain_1in", "Probability of 3-hour Rain over 1 in", "prob", "APCP", surface, Time::ThreeHour, "prob >25.4", 1, Look::Prob, "Chance of more than 1 in of rain in 3 hours (%)", false},
        {"eas_prob_1h_rain_0.01in", "Ensemble Agreement Scale Probability of 1-hour Rain over 0.01 in", "eas", "APCP", surface, Time::OneHour, "prob >0.254", 1, Look::Prob, "Neighborhood chance of more than 0.01 in of rain in the hour (%)", false},
        {"eas_prob_1h_rain_0.25in", "Ensemble Agreement Scale Probability of 1-hour Rain over 0.25 in", "eas", "APCP", surface, Time::OneHour, "prob >6.35", 1, Look::Prob, "Neighborhood chance of more than 0.25 in of rain in the hour (%)", false},
        {"eas_prob_1h_rain_0.5in", "Ensemble Agreement Scale Probability of 1-hour Rain over 0.5 in", "eas", "APCP", surface, Time::OneHour, "prob >12.7", 1, Look::Prob, "Neighborhood chance of more than 0.5 in of rain in the hour (%)", false},
        {"eas_prob_3h_rain_0.01in", "Ensemble Agreement Scale Probability of 3-hour Rain over 0.01 in", "eas", "APCP", surface, Time::ThreeHour, "prob >0.254", 1, Look::Prob, "Neighborhood chance of more than 0.01 in of rain in 3 hours (%)", false},
        {"eas_prob_3h_rain_0.25in", "Ensemble Agreement Scale Probability of 3-hour Rain over 0.25 in", "eas", "APCP", surface, Time::ThreeHour, "prob >6.35", 1, Look::Prob, "Neighborhood chance of more than 0.25 in of rain in 3 hours (%)", false},
        {"eas_prob_3h_rain_0.5in", "Ensemble Agreement Scale Probability of 3-hour Rain over 0.5 in", "eas", "APCP", surface, Time::ThreeHour, "prob >12.7", 1, Look::Prob, "Neighborhood chance of more than 0.5 in of rain in 3 hours (%)", false},
        {"mean_snow_1h", "Mean 1-hour Snow (10 to 1 ratio)", "mean", "WEASD", surface, Time::OneHour, "", 1, Look::Snow, "Mean snowfall in the hour, from the water at 10 to 1", false},
        {"mean_snow_3h", "Mean 3-hour Snow (10 to 1 ratio)", "mean", "WEASD", surface, Time::ThreeHour, "", 1, Look::Snow, "Mean snowfall in 3 hours, from the water at 10 to 1", false},
        {"mean_snow_total", "Mean Total Snow (10 to 1 ratio)", "mean", "WEASD", surface, Time::Total, "", 1, Look::Snow, "Mean snowfall since the start of the run, from the water at 10 to 1", false},
        {"prob_1h_snow_1in", "Probability of 1-hour Snow over 1 in", "prob", "WEASD", surface, Time::OneHour, "prob >2.54", 1, Look::Prob, "Chance of more than 1 in of snow in the hour (%)", false},
        {"prob_3h_snow_1in", "Probability of 3-hour Snow over 1 in", "prob", "WEASD", surface, Time::ThreeHour, "prob >2.54", 1, Look::Prob, "Chance of more than 1 in of snow in 3 hours (%)", false},
        {"prob_3h_snow_3in", "Probability of 3-hour Snow over 3 in", "prob", "WEASD", surface, Time::ThreeHour, "prob >7.62", 1, Look::Prob, "Chance of more than 3 in of snow in 3 hours (%)", false},
        {"eas_prob_1h_snow_0.1in", "Ensemble Agreement Scale Probability of 1-hour Snow over 0.1 in", "eas", "WEASD", surface, Time::OneHour, "prob >2.54", 1, Look::Prob, "Neighborhood chance of more than 0.1 in of snow in the hour (%)", false},
        {"eas_prob_1h_snow_0.3in", "Ensemble Agreement Scale Probability of 1-hour Snow over 0.3 in", "eas", "WEASD", surface, Time::OneHour, "prob >7.62", 1, Look::Prob, "Neighborhood chance of more than 0.3 in of snow in the hour (%)", false},
        {"eas_prob_3h_snow_0.1in", "Ensemble Agreement Scale Probability of 3-hour Snow over 0.1 in", "eas", "WEASD", surface, Time::ThreeHour, "prob >2.54", 1, Look::Prob, "Neighborhood chance of more than 0.1 in of snow in 3 hours (%)", false},
        {"eas_prob_3h_snow_0.3in", "Ensemble Agreement Scale Probability of 3-hour Snow over 0.3 in", "eas", "WEASD", surface, Time::ThreeHour, "prob >7.62", 1, Look::Prob, "Neighborhood chance of more than 0.3 in of snow in 3 hours (%)", false},
        {"mean_pwat", "Mean Precipitable Water", "mean", "PWAT", column, Time::Instant, "", 1, Look::Pwat, "Mean precipitable water", false},
        {"prob_pwat_1.5in", "Probability of Precipitable Water over 1.5 in", "prob", "PWAT", column, Time::Instant, "prob >37.5", 1, Look::Prob, "Chance of precipitable water over 1.5 in (%)", false},
        {"prob_pwat_2in", "Probability of Precipitable Water over 2 in", "prob", "PWAT", column, Time::Instant, "prob >50", 1, Look::Prob, "Chance of precipitable water over 2 in (%)", false},
        {"mean_2m_temp", "Mean 2 m Temperature", "mean", "TMP", "2 m above ground", Time::Instant, "", 1, Look::Temp, "Mean 2 m temperature", false},
        {"prob_2m_temp_0C", "Probability of 2 m Temperature under 0 C", "prob", "TMP", "2 m above ground", Time::Instant, "prob <273.15", 1, Look::Prob, "Chance of a 2 m temperature under 0 C (%)", false},
        {"mean_2m_dewp", "Mean 2 m Dew Point", "mean", "DPT", "2 m above ground", Time::Instant, "", 1, Look::Dew, "Mean 2 m dew point", false},
        {"prob_2m_dewp_55F", "Probability of 2 m Dew Point over 55 F", "prob", "DPT", "2 m above ground", Time::Instant, "prob >285.93", 1, Look::Prob, "Chance of a 2 m dew point over 55 F (%)", false},
        {"prob_2m_dewp_65F", "Probability of 2 m Dew Point over 65 F", "prob", "DPT", "2 m above ground", Time::Instant, "prob >291.48", 1, Look::Prob, "Chance of a 2 m dew point over 65 F (%)", false},
        {"prob_10m_wspd_20kt", "Probability of 10 m Wind Speed over 20 kt", "prob", "WIND", "10 m above ground", Time::Instant, "prob >10.3", 1, Look::Prob, "Chance of a 10 m wind over 20 kt (%)", false},
        {"prob_10m_wspd_30kt", "Probability of 10 m Wind Speed over 30 kt", "prob", "WIND", "10 m above ground", Time::Instant, "prob >15.4", 1, Look::Prob, "Chance of a 10 m wind over 30 kt (%)", false},
        {"mean_vis", "Mean Visibility", "mean", "VIS", surface, Time::Instant, "", 1.0 / 1609.344, Look::Vis, "Mean visibility (miles)", false},
        {"prob_vis_0.5mi", "Probability of Visibility under 0.5 mile", "prob", "VIS", surface, Time::Instant, "prob <800", 1, Look::Prob, "Chance of a visibility under half a mile (%)", false},
        {"prob_etop_30000ft", "Probability of Echo Top over 30,000 ft", "prob", "RETOP", column, Time::Instant, "prob >9144", 1, Look::Prob, "Chance of an echo top over 30,000 ft (%)", false},
        {"prob_etop_35000ft", "Probability of Echo Top over 35,000 ft", "prob", "RETOP", column, Time::Instant, "prob >10668", 1, Look::Prob, "Chance of an echo top over 35,000 ft (%)", false},
        {"prob_ceil_1000ft", "Probability of Ceiling under 1,000 ft", "prob", "HGT", "cloud ceiling", Time::Instant, "prob <305", 1, Look::Prob, "Chance of a ceiling under 1,000 ft (%)", false},
        {"prob_ceil_2000ft", "Probability of Ceiling under 2,000 ft", "prob", "HGT", "cloud ceiling", Time::Instant, "prob <610", 1, Look::Prob, "Chance of a ceiling under 2,000 ft (%)", false},
        {"prob_ceil_3000ft", "Probability of Ceiling under 3,000 ft", "prob", "HGT", "cloud ceiling", Time::Instant, "prob <915", 1, Look::Prob, "Chance of a ceiling under 3,000 ft (%)", false},
        {"prob_low_IFR", "Probability of Low Instrument Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=1 <2", 1, Look::Prob, "Chance of low IFR (%)", false},
        {"prob_IFR", "Probability of Instrument Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=2 <3", 1, Look::Prob, "Chance of IFR (%)", false},
        {"prob_marginal_VFR", "Probability of Marginal Visual Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=3 <4", 1, Look::Prob, "Chance of marginal VFR (%)", false},
        {"prob_VFR", "Probability of Visual Flight Rules", "prob", "FLGHT", surface, Time::Instant, "prob >=4 <0", 1, Look::Prob, "Chance of VFR (%)", false},
        {"mean_ml_cape", "Mean Mixed Layer CAPE", "mean", "CAPE", "90-0 mb above ground", Time::Instant, "", 1, Look::Cape, "Mean mixed-layer CAPE (J/kg)", false},
        {"prob_cape_500", "Probability of Mixed Layer CAPE over 500", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >500", 1, Look::Prob, "Chance of mixed-layer CAPE over 500 J/kg (%)", false},
        {"prob_cape_1000", "Probability of Mixed Layer CAPE over 1000", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >1000", 1, Look::Prob, "Chance of mixed-layer CAPE over 1000 J/kg (%)", false},
        {"prob_cape_2000", "Probability of Mixed Layer CAPE over 2000", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >2000", 1, Look::Prob, "Chance of mixed-layer CAPE over 2000 J/kg (%)", false},
        {"prob_cape_3000", "Probability of Mixed Layer CAPE over 3000", "prob", "CAPE", "90-0 mb above ground", Time::Instant, "prob >3000", 1, Look::Prob, "Chance of mixed-layer CAPE over 3000 J/kg (%)", false},
        {"mean_vwshr", "Mean 0-6 km Vertical Wind Shear", "mean", "VWSH", "0-6000 m above ground", Time::Instant, "", 1.943844, Look::Shear, "Mean 0-6 km wind shear (kt)", false},
        {"prob_vwshr_30kt", "Probability of 0-6 km Vertical Wind Shear over 30 kt", "prob", "VWSH", "0-6000 m above ground", Time::Instant, "prob >15.4", 1, Look::Prob, "Chance of 0-6 km wind shear over 30 kt (%)", false},
        {"prob_max_hlcy_25", "Probability of Maximum 2-5 km Updraft Helicity over 25", "prob", "MXUPHL", "5000-2000 m above ground", Time::Instant, "prob >25", 1, Look::Prob, "Chance of updraft helicity over 25 (%)", false},
        {"prob_max_hlcy_75", "Probability of Maximum 2-5 km Updraft Helicity over 75", "prob", "MXUPHL", "5000-2000 m above ground", Time::Instant, "prob >75", 1, Look::Prob, "Chance of updraft helicity over 75 (%)", false},
        {"prob_max_hlcy_150", "Probability of Maximum 2-5 km Updraft Helicity over 150", "prob", "MXUPHL", "5000-2000 m above ground", Time::Instant, "prob >150", 1, Look::Prob, "Chance of updraft helicity over 150 (%)", false},
    };
    for (const auto& row : rows) {
        Product x;
        x.source = "HREF";
        x.id = row.id;
        x.label = row.label;
        const std::string kind = row.kind, variable = row.variable, level = row.level, detail = row.detail;
        const Time time = row.time;
        const bool pressureLines = row.pressure;
        x.needs = [kind, variable, level, detail, time, pressureLines] (int hour) {
            std::vector<GfsData::Need> out;
            const auto accumulated = [] (int from, int to) { return std::to_string(from) + "-" + std::to_string(to) + " hour acc fcst"; };
            if (time == Time::Instant) {
                out.push_back({hour, GfsData::Want{"f", variable, level, std::to_string(hour) + " hour fcst", detail, kind}});
            } else if (time == Time::OneHour && hour >= 1) {
                out.push_back({hour, GfsData::Want{"f", variable, level, accumulated(hour - 1, hour), detail, kind}});
            } else if (time == Time::ThreeHour && hour >= 3) {
                out.push_back({hour, GfsData::Want{"f", variable, level, accumulated(hour - 3, hour), detail, kind}});
            } else if (time == Time::Total) {   // the 3 hour amounts back to the start, added up
                int n = 0;
                for (int end = hour; end > 0; end -= 3) {
                    out.push_back({end, GfsData::Want{"a" + std::to_string(n++), variable, level, accumulated(std::max(end - 3, 0), end), detail, kind}});
                }
            }
            if (pressureLines && !out.empty()) {
                out.push_back({hour, GfsData::Want{"p", "MSLET", "mean sea level", std::to_string(hour) + " hour fcst", "", "mean"}});
            }
            return out;
        };
        const double scale = row.scale;
        x.derive = [time, scale] (Grids& g, const Context&) {
            if (time == Time::Total) {
                auto sum = g["a0"];
                for (int k = 1; g.count("a" + std::to_string(k)); k++) {
                    const auto& piece = g["a" + std::to_string(k)];
                    for (size_t i = 0; i < sum.values.size(); i++) {
                        sum.values[i] += piece.values[i];
                    }
                }
                g["f"] = std::move(sum);
            }
            if (scale != 1.0 && g.count("f")) {
                g["f"] = GfsGrid::scaled(g["f"], scale);
            }
        };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.fillTitle = row.title;
        x.legendStep = 0;
        switch (row.look) {
            case Look::Precip: x.ramp = precipitation(); x.palette = "precip"; x.quantity = Quantity::Millimeters; break;
            case Look::Snow: x.ramp = snowfall(); x.quantity = Quantity::Centimeters; break;
            case Look::Radar: x.ramp = reflectivity(); x.palette = "radar_rain"; break;
            case Look::Prob: x.ramp = probability(); x.palette = "prob"; break;
            case Look::Temp: x.ramp = temperature(); x.quantity = Quantity::Temperature; x.legendStep = 5; break;
            case Look::Dew: x.ramp = dewpoint(); x.quantity = Quantity::Temperature; x.legendStep = 5; break;
            case Look::Pwat: x.ramp = precipitableWater(); x.quantity = Quantity::Millimeters; break;
            case Look::Cape: x.ramp = capeRamp(); x.legendStep = 500; break;
            case Look::Vis: x.ramp = visibilityRamp(); x.palette = "vis"; break;
            case Look::Shear: x.ramp = shearRamp(); x.legendStep = 10; break;
            case Look::Plain: x.ramp = humidity(); break;
        }
        if (pressureLines) {
            auto lines = pressure();
            lines.interval = 2;   // the model guidance site's 2 mb
            lines.highsAndLows = false;
            x.contours = {lines};
        }
        p.push_back(x);
    }
    {   // the low instrument flight rules and the instrument flight rules together: the second as the fill, the first as lines at 30, 60 and 90 per cent
        Product x;
        x.source = "HREF";
        x.id = "prob_lowIFR_IFR";
        x.label = "Probability of Low Instrument Flight Rules and Instrument Flight Rules";
        x.needs = [] (int hour) {
            const auto when = std::to_string(hour) + " hour fcst";
            return std::vector<GfsData::Need>{{hour, GfsData::Want{"f", "FLGHT", "surface", when, "prob >=2 <3", "prob"}}, {hour, GfsData::Want{"l", "FLGHT", "surface", when, "prob >=1 <2", "prob"}}};
        };
        x.fill = [] (const Grids& g) { return pick(g, "f"); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = "Chance of IFR (%, fill); lines: chance of low IFR";
        x.legendStep = 0;
        ContourSet low;
        low.key = "l";
        low.base = 30;
        low.minimum = 30;
        low.interval = 30;
        low.title = "Chance of low IFR (%)";
        low.color = QColor{110, 20, 130};
        low.width = 2.0;
        x.contours = {low};
        p.push_back(x);
    }
}

}  // namespace detail
}  // namespace GfsChart
