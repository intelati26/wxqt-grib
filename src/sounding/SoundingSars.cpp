// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingSars.h"
#include <cmath>

namespace SoundingSars {
    namespace {
        // SHARPpy tests value >= (match - range) and value <= (match + range)
        bool within(double value, double match, double range) { return value >= match - range && value <= match + range; }
    }

    Result supercell(double mlcape, double mllcl, double temp500, double lapse, double shear6, double srh1, double shear3,
                     double shear9, double srh3) {
        Result result;
        const double rangeMlcape = mlcape == 0.0 ? 0.0 : 1300.0;
        const double rangeMlcapeT1 = mlcape * 0.25;
        constexpr double rangeMllcl = 500.0;
        constexpr double rangeMllclT1 = 200.0;
        constexpr double rangeShr = 14.0;
        constexpr double rangeShrT1 = 10.0;
        const double rangeSrh = std::fabs(srh1) < 50.0 ? 100.0 : srh1;   // as SHARPpy writes it (not the absolute value)
        const double rangeSrhT1 = std::fabs(srh1) < 100.0 ? 50.0 : std::fabs(srh1) * 0.30;
        const double rangeSrh3T1 = std::fabs(srh3) < 100.0 ? 50.0 : std::fabs(srh3) * 0.50;
        constexpr double rangeTemp = 7.0;
        constexpr double rangeTempT1 = 5.0;
        constexpr double rangeLr = 1.0;
        constexpr double rangeLrT1 = 0.8;
        constexpr double rangeShr3T1 = 15.0;
        constexpr double rangeShr9T1 = 25.0;
        int loose = 0;
        int tornadoes = 0;
        for (const auto& r : supercellDatabase()) {
            if (within(mlcape, r.mlcape, rangeMlcape) && within(mllcl, r.mllcl, rangeMllcl) && within(shear6, r.shear6, rangeShr) &&
                within(srh1, r.srh1, rangeSrh) && within(temp500, r.temp500, rangeTemp) && within(lapse, r.lapse75, rangeLr)) {
                loose += 1;
                if (r.category > 0) {
                    tornadoes += 1;
                }
            }
            if (within(mlcape, r.mlcape, rangeMlcapeT1) && within(mllcl, r.mllcl, rangeMllclT1) && within(shear6, r.shear6, rangeShrT1) &&
                within(srh1, r.srh1, rangeSrhT1) && within(temp500, r.temp500, rangeTempT1) && within(lapse, r.lapse75, rangeLrT1) &&
                within(shear3, r.shear3, rangeShr3T1) && within(shear9, r.shear9, rangeShr9T1) && within(srh3, r.srh3, rangeSrh3T1)) {
                Match match;
                match.id = r.id;
                match.category = r.category;
                match.label = r.category == 2 ? "SIGTOR" : (r.category == 1 ? "WEAKTOR" : "NONTOR");
                result.quality.push_back(match);
            }
        }
        result.looseMatches = loose;
        result.significant = tornadoes;
        result.probability = (loose > 0 && mlcape > 0.0) ? static_cast<double>(tornadoes) / loose : 0.0;
        result.valid = true;
        return result;
    }

    Result hail(double mumr, double mucape, double temp500, double lapse, double shear6, double shear9, double shear3, double srh3) {
        Result result;
        constexpr double rangeMumr = 2.0;
        const double rangeMucape = mucape * 0.30;
        const double rangeMucapeT1 = mucape < 500.0 ? mucape * 0.50 : (mucape < 2000.0 ? mucape * 0.25 : mucape * 0.20);
        constexpr double rangeLr = 2.0;
        constexpr double rangeLrT1 = 0.4;
        constexpr double rangeTemp = 9.0;
        constexpr double rangeTempT1 = 1.5;
        constexpr double rangeShr6 = 12.0;
        constexpr double rangeShr6T1 = 6.0;
        constexpr double rangeShr9 = 22.0;
        constexpr double rangeShr9T1 = 15.0;
        constexpr double rangeShr3 = 10.0;
        constexpr double rangeShr3T1 = 8.0;
        const double rangeSrhT1 = srh3 < 50.0 ? 25.0 : srh3 * 0.5;
        constexpr size_t maxQuality = 15;   // SPC's graphic does not list more
        int loose = 0;
        int significant = 0;
        for (const auto& r : hailDatabase()) {
            if (within(mumr, r.mumr, rangeMumr) && within(mucape, r.mucape, rangeMucape) && within(lapse, r.lapse75, rangeLr) &&
                within(temp500, r.temp500, rangeTemp) && within(shear6, r.shear6, rangeShr6) && within(shear9, r.shear9, rangeShr9) &&
                within(shear3, r.shear3, rangeShr3)) {
                loose += 1;
                if (r.size >= 2.0f) {
                    significant += 1;
                }
            }
            if (result.quality.size() < maxQuality && within(mumr, r.mumr, rangeMumr) && within(mucape, r.mucape, rangeMucapeT1) &&
                within(lapse, r.lapse75, rangeLrT1) && within(temp500, r.temp500, rangeTempT1) && within(shear6, r.shear6, rangeShr6T1) &&
                within(shear9, r.shear9, rangeShr9T1) && within(shear3, r.shear3, rangeShr3T1) && within(srh3, r.srh3, rangeSrhT1)) {
                Match match;
                match.id = r.id;
                match.size = r.size;
                result.quality.push_back(match);
            }
        }
        result.looseMatches = loose;
        result.significant = significant;
        result.probability = (loose > 0 && mucape > 0.0) ? static_cast<double>(significant) / loose : 0.0;
        result.valid = true;
        return result;
    }
}
