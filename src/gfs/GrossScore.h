// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GROSSSCORE_H
#define GROSSSCORE_H

#include <algorithm>
#include <cmath>

// The Gross to Miserable weather index of one hour (the user's specification, gross-weather-index-formula.md): the discomfort of the weather as a score from 0 up. Header only, with no Qt, so a test can
// hold it to the specification. Temperatures in degrees F, wind in mph, the chances and the relative humidity in percent; NaN where a value is not known (that part counts nothing).
namespace GrossScore {
    // The feels like lines past which the temperature counts: the specification's 45 and 85 F moved half as far as the place's normal high (NaN: unknown) is from 70 F (the user, 2026-11-16: a
    // little of the normal), and feels like over 100 F is always hot (the hot line never above 100).
    inline void lines(double normalHigh, double& cold, double& hot) {
        const double shift = std::isnan(normalHigh) ? 0.0 : 0.5 * (normalHigh - 70.0);
        cold = 45.0 + shift;
        hot = std::min(100.0, 85.0 + shift);
    }

    // The air temperature again from what the blend gives anyway, so that its own temperature and relative humidity need not be fetched (3.4 MB an hour of 14.6, a quarter of what a day of
    // the outlook downloads): the blend's feels like (APTMP) is the wind chill below 50 F with wind over 3 mph, which is a formula that can be turned round, and the sustained wind is
    // in the day already. Measured on the 12Z run of 2026-11-18, nine hours, 3.7 million cells: the temperature is found to 0.05 F on average (0.15 F at the 99th percentile) and the
    // Gross tier of a cell changed in 0.002% of the cells. Above 50 F, or in a light wind, the feels like is the temperature as far as this score cares (it uses the temperature only
    // below 50 F: chilly dampness).
    inline double temperatureFrom(double feels, double windMph) {
        if (std::isnan(feels) || std::isnan(windMph) || windMph <= 3.0) {
            return feels;
        }
        const double v = std::pow(windMph, 0.16);
        const double t = (feels - 35.74 + 35.75 * v) / (0.6215 + 0.4275 * v);
        return t <= 50.0 ? t : feels;
    }
    // The relative humidity (percent) from the temperature and the dew point (F), the Magnus formula: within 0.3 of the blend's own humidity on average, so that the chilly dampness line
    // (over 85%) falls on the other side for 0.5% of the cells (the ones within a few tenths of it).
    inline double humidityFrom(double tempF, double dewF) {
        if (std::isnan(tempF) || std::isnan(dewF)) {
            return std::nan("");
        }
        const double t = (tempF - 32.0) / 1.8, d = (dewF - 32.0) / 1.8;
        return std::min(100.0, 100.0 * std::exp(17.625 * d / (243.04 + d) - 17.625 * t / (243.04 + t)));
    }

    struct Hour {
        double feels{std::nan("")}, dew{std::nan("")}, temp{std::nan("")}, humidity{std::nan("")}, wind{std::nan("")};
        double rainChance{std::nan("")};   // the chance of precipitation
        double icyChance{std::nan("")};    // the chance of freezing rain plus ice pellets
    };

    inline double score(const Hour& h, double coldLine = 45.0, double hotLine = 85.0) {
        double s = 0.0;
        s += h.feels > hotLine ? 1.5 * (h.feels - hotLine) : h.feels < coldLine ? 1.5 * (coldLine - h.feels) : 0.0;
        s += h.dew > 65.0 ? 2.5 * (h.dew - 65.0) : 0.0;
        if (!std::isnan(h.rainChance) && h.rainChance >= 50.0) {   // likely precipitation, or 40 when freezing rain or sleet is the likely kind
            s += !std::isnan(h.icyChance) && h.icyChance >= 50.0 ? 40.0 : 15.0;
        }
        if (!std::isnan(h.temp) && !std::isnan(h.humidity) && h.temp < 50.0 && h.humidity > 85.0) {   // chilly dampness
            s += 15.0;
        }
        s += !std::isnan(h.wind) && h.wind > 15.0 ? 1.2 * (h.wind - 15.0) : 0.0;
        s += h.feels >= 105.0 || h.feels <= -10.0 ? 50.0 : 0.0;   // the extremes, whatever is normal
        return s;
    }

    // 1 Mild (0 to 19), 2 Gross (20 to 44), 3 Nasty (45 to 69), 4 Miserable (70 and over)
    inline int tier(double score) {
        return score >= 70.0 ? 4 : score >= 45.0 ? 3 : score >= 20.0 ? 2 : 1;
    }
}

#endif  // GROSSSCORE_H
