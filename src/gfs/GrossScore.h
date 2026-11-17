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
