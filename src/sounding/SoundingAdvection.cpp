// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingAdvection.h"
#include <cmath>

namespace SoundingAdvection {
    namespace {
        constexpr double pi = 3.14159265358979323846;
        constexpr double knotsToMs = 0.514444;
        bool gone(double v) { return v <= -9998.0; }

        // winds.mean_wind: pressure-weighted mean of u and v on 1 mb steps from pBottom to pTop (inclusive)
        bool meanWind(const SoundingProfile& p, double pBottom, double pTop, double& u, double& v) {
            double su = 0.0;
            double sv = 0.0;
            double sw = 0.0;
            for (double pr = pBottom; pr >= pTop - 1e-9; pr -= 1.0) {
                double uu;
                double vv;
                if (!p.interpComponents(pr, uu, vv)) {
                    continue;
                }
                su += uu * pr;
                sv += vv * pr;
                sw += pr;
            }
            if (sw <= 0.0) {
                return false;
            }
            u = su / sw;
            v = sv / sw;
            return true;
        }

        // meteorological direction (degrees the wind blows from) of a u / v pair
        double direction(double u, double v) {
            double d = std::atan2(-u, -v) * 180.0 / pi;
            return d < 0.0 ? d + 360.0 : d;
        }
    }

    std::vector<Layer> inferred(const SoundingProfile& p, double latitude) {
        std::vector<Layer> layers;
        if (p.size() == 0 || gone(p.sfcPres())) {
            return layers;
        }
        // the lowest pressure that is still 100 mb or more: np.arange stops short of it
        double lastPressure = -1.0;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (!gone(p.pres[i]) && p.pres[i] >= 100.0) {
                lastPressure = p.pres[i];
            }
        }
        if (lastPressure < 0.0) {
            return layers;
        }
        std::vector<double> levels;
        for (double pr = p.sfcPres(); pr > lastPressure; pr -= 100.0) {
            levels.push_back(pr);
        }
        const double omega = 2.0 * pi / 86164.0;
        const double f = 2.0 * omega * std::sin(latitude * pi / 180.0);
        const double multiplier = (f / 9.81) * (pi / 180.0);
        for (size_t i = 1; i < levels.size(); i += 1) {
            const double bottomPres = levels[i - 1];
            const double topPres = levels[i];
            const double bottomTemp = p.interpTemp(bottomPres);
            const double topTemp = p.interpTemp(topPres);
            const double bottomHght = p.interpHght(bottomPres);
            const double topHght = p.interpHght(topPres);
            double bu;
            double bv;
            double tu;
            double tv;
            double mu;
            double mv;
            if (gone(bottomTemp) || gone(topTemp) || gone(bottomHght) || gone(topHght) || topHght == bottomHght ||
                !p.interpComponents(bottomPres, bu, bv) || !p.interpComponents(topPres, tu, tv) ||
                !meanWind(p, bottomPres, topPres, mu, mv)) {
                continue;
            }
            const double bottomDir = direction(bu, bv);
            double topDir = direction(tu, tv);
            const double meanSpeed = std::hypot(mu, mv) * knotsToMs;
            // the change of wind direction with height decides warm or cold advection: turn the bottom wind to 180
            topDir += 180.0 - bottomDir;
            if (topDir < 0.0) {
                topDir += 360.0;
            } else if (topDir >= 360.0) {
                topDir -= 360.0;
            }
            const double dTheta = topDir - 180.0;
            // SHARPpy's "average" temperature is the sum of the two times 2 (kept exactly as it is there)
            const double avgTemp = ((topTemp + 273.15) + (bottomTemp + 273.15)) * 2.0;
            const double perSecond = multiplier * meanSpeed * meanSpeed * avgTemp * (dTheta / (topHght - bottomHght));
            layers.push_back({bottomPres, topPres, perSecond * 3600.0});
        }
        return layers;
    }
}
