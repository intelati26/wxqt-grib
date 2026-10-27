// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/Ec46Loader.h"
#include <cmath>
#include "cfs/UtilityOpenMeteoSeasonal.h"
#include "gfs/GfsData.h"
#include "gfs/GfsGrid.h"

namespace {
    constexpr double west = -135.0, south = 14.0, east = -55.0, north = 58.0, step = 4.0, fine = 0.5;   // (252 points: the service allows about 600 a minute, so two fields a minute)

    bool load(const GfsData::Run&, int forecastHour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        const auto week = static_cast<size_t>(std::max(0, forecastHour) / 168);
        UtilityOpenMeteoSeasonal::PeriodGrid g;
        if (!UtilityOpenMeteoSeasonal::periodGrid(true, want.variable, west, south, east, north, step, g, error)) {
            return false;
        }
        if (week >= g.starts.size()) {
            error = "the EC46 forecast has no week " + std::to_string(week + 1) + " yet";
            return false;
        }
        GfsGrid::Grid coarse;
        coarse.columns = g.columns;
        coarse.rows = g.rows;
        coarse.lon0 = g.west;
        coarse.lat0 = g.north;
        coarse.step = g.step;
        coarse.values = g.values[week];
        // smooth: the weekly mean of an ensemble at a 36 km model varies slowly, and the chart colors the values between the points
        out.columns = static_cast<int>(std::lround((east - west) / fine)) + 1;
        out.rows = static_cast<int>(std::lround((north - south) / fine)) + 1;
        out.lon0 = west;
        out.lat0 = north;
        out.step = fine;
        out.values.assign(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows), std::nanf(""));
        for (int r = 0; r < out.rows; r++) {
            const double lat = std::clamp(north - r * fine, coarse.lat0 - (coarse.rows - 1) * coarse.step, coarse.lat0);
            for (int c = 0; c < out.columns; c++) {
                const double lon = std::clamp(west + c * fine, coarse.lon0, coarse.lon0 + (coarse.columns - 1) * coarse.step);
                out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = coarse.sampleCubic(lon, lat);
            }
        }
        return true;
    }
}

void Ec46Loader::registerLoader() {
    GfsData::setLoader("EC46", load);
}
