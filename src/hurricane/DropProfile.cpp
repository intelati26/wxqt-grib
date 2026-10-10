// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/DropProfile.h"
#include <cmath>
#include <cstdio>
#include "hurricane/UtilityHdob.h"

namespace {
    // fills the gaps of one quantity between the levels that have it: linear in the log of pressure (the way the code defines its significant levels);
    // the levels above the last or below the first that have it stay missing
    void fill(const std::vector<double>& pressure, std::vector<double>& value) {
        std::vector<size_t> have;
        for (size_t i = 0; i < value.size(); i++) {
            if (UtilityDropsonde::has(value[i])) {
                have.push_back(i);
            }
        }
        for (size_t k = 0; k + 1 < have.size(); k++) {
            const size_t a = have[k];
            const size_t b = have[k + 1];
            for (size_t i = a + 1; i < b; i++) {
                const double f = (std::log(pressure[i]) - std::log(pressure[a])) / (std::log(pressure[b]) - std::log(pressure[a]));
                value[i] = value[a] + (value[b] - value[a]) * f;
            }
        }
    }
}

bool DropProfile::build(const UtilityDropsonde::Drop& drop, SoundingProfile& out, std::string& error) {
    out = SoundingProfile{};
    // the levels from the surface up (the mandatory 1000 hPa level can lie below it)
    std::vector<UtilityDropsonde::Level> levels;
    for (const auto& l : drop.levels) {
        if (!UtilityDropsonde::has(drop.surfacePressure) || l.pressure <= drop.surfacePressure + 0.5) {
            levels.push_back(l);
        }
    }
    // the levels that have a reported height
    std::vector<size_t> known;
    for (size_t i = 0; i < levels.size(); i++) {
        if (UtilityDropsonde::has(levels[i].height)) {
            known.push_back(i);
        }
    }
    if (known.size() < 3) {
        error = "This dropsonde report has too few levels with heights to draw a sounding.";
        return false;
    }
    const size_t top = known.back();
    std::vector<double> pressure, height, temperature, dewPoint, u, v;
    for (size_t i = 0; i <= top; i++) {
        const auto& l = levels[i];
        double h = l.height;
        if (!UtilityDropsonde::has(h)) {
            // between the nearest reported heights below and above, linear in the log of pressure
            size_t below = 0;
            size_t above = known.back();
            for (const auto k : known) {
                if (k < i) {
                    below = k;
                } else if (k > i) {
                    above = k;
                    break;
                }
            }
            const double p0 = std::log(levels[below].pressure);
            const double p1 = std::log(levels[above].pressure);
            h = levels[below].height + (levels[above].height - levels[below].height) * (std::log(l.pressure) - p0) / (p1 - p0);
        }
        pressure.push_back(l.pressure);
        height.push_back(h);
        temperature.push_back(l.temperature);
        dewPoint.push_back(l.dewPoint);
        // the wind as components, so a direction is interpolated sensibly
        if (UtilityDropsonde::has(l.windSpeed) && UtilityDropsonde::has(l.windDirection)) {
            const double rad = std::acos(-1.0) / 180.0;
            u.push_back(-l.windSpeed * std::sin(l.windDirection * rad));
            v.push_back(-l.windSpeed * std::cos(l.windDirection * rad));
        } else {
            u.push_back(UtilityDropsonde::missing);
            v.push_back(UtilityDropsonde::missing);
        }
    }
    fill(pressure, temperature);
    fill(pressure, dewPoint);
    fill(pressure, u);
    fill(pressure, v);
    for (size_t i = 0; i < pressure.size(); i++) {
        out.pres.push_back(pressure[i]);
        out.hght.push_back(height[i]);
        out.tmpc.push_back(temperature[i]);
        out.dwpc.push_back(dewPoint[i]);
        if (UtilityDropsonde::has(u[i]) && UtilityDropsonde::has(v[i])) {
            const double rad = std::acos(-1.0) / 180.0;
            const double speed = std::hypot(u[i], v[i]);
            double direction = std::atan2(-u[i], -v[i]) / rad;
            if (direction < 0.0) {
                direction += 360.0;
            }
            out.wdir.push_back(direction);
            out.wspd.push_back(speed);
        } else {
            out.wdir.push_back(UtilityDropsonde::missing);
            out.wspd.push_back(UtilityDropsonde::missing);
        }
    }
    out.station = "Dropsonde";
    out.latitude = UtilityDropsonde::has(drop.lat) ? drop.lat : 25.0;
    out.validTime = UtilityHdob::timeText(drop.seconds);
    out.finalize();
    return out.size() >= 3;
}

std::string DropProfile::title(const UtilityDropsonde::Drop& drop) {
    char position[48] = "";
    if (UtilityDropsonde::has(drop.lat)) {
        std::snprintf(position, sizeof position, "  %.1f%c %.1f%c", std::abs(drop.lat), drop.lat >= 0 ? 'N' : 'S', std::abs(drop.lon), drop.lon >= 0 ? 'E' : 'W');
    }
    return "Dropsonde " + drop.mission + "  " + UtilityHdob::timeText(drop.seconds) + position;
}
