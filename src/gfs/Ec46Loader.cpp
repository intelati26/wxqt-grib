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
    // the world in coarse points (the service allows about 600 points a minute and 5,000 an hour, so the world takes a minute and a half a field), with the United States in finer ones inside it
    constexpr double worldWest = -180.0, worldSouth = -60.0, worldEast = 180.0, worldNorth = 80.0, worldStep = 10.0;
    constexpr double west = -135.0, south = 14.0, east = -55.0, north = 58.0, step = 4.0, fine = 0.5;

    // a coarse grid of the service's points as a GfsGrid
    GfsGrid::Grid coarseOf(const UtilityOpenMeteoSeasonal::PeriodGrid& g, size_t week) {
        GfsGrid::Grid coarse;
        coarse.columns = g.columns;
        coarse.rows = g.rows;
        coarse.lon0 = g.west;
        coarse.lat0 = g.north;
        coarse.step = g.step;
        coarse.values = g.values[week];
        return coarse;
    }

    // a forecast "hour" is a period of the model: 168 hours a week for the EC46, 720 a month for the SEAS5
    bool loadPeriod(bool weekly, int forecastHour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        const auto week = static_cast<size_t>(std::max(0, forecastHour) / (weekly ? 168 : 720));
        UtilityOpenMeteoSeasonal::PeriodGrid world;
        if (!UtilityOpenMeteoSeasonal::periodGrid(weekly, want.variable, worldWest, worldSouth, worldEast, worldNorth, worldStep, world, error)) {
            return false;
        }
        if (week >= world.starts.size()) {
            error = std::string{"the "} + (weekly ? "EC46 forecast has no week " : "SEAS5 forecast has no month ") + std::to_string(week + 1) + " yet";
            return false;
        }
        const auto coarseWorld = coarseOf(world, week);
        UtilityOpenMeteoSeasonal::PeriodGrid us;
        std::string ignored;
        const bool haveUs = UtilityOpenMeteoSeasonal::periodGrid(weekly, want.variable, west, south, east, north, step, us, ignored) && week < us.starts.size();   // (the world alone when the service's minute is spent)
        GfsGrid::Grid coarseUs;
        if (haveUs) {
            coarseUs = coarseOf(us, week);
        }
        // smooth: the weekly mean of an ensemble at a 36 km model varies slowly, and the chart colors the values between the points
        out.columns = static_cast<int>(std::lround((worldEast - worldWest) / fine)) + 1;
        out.rows = static_cast<int>(std::lround((worldNorth - worldSouth) / fine)) + 1;
        out.lon0 = worldWest;
        out.lat0 = worldNorth;
        out.step = fine;
        out.values.assign(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows), std::nanf(""));
        for (int r = 0; r < out.rows; r++) {
            const double lat = worldNorth - r * fine;
            for (int c = 0; c < out.columns; c++) {
                const double lon = worldWest + c * fine;
                const bool inUs = haveUs && lat >= south && lat <= north && lon >= west && lon <= east;
                const auto& source = inUs ? coarseUs : coarseWorld;
                const double la = std::clamp(lat, source.lat0 - (source.rows - 1) * source.step, source.lat0);
                const double lo = std::clamp(lon, source.lon0, source.lon0 + (source.columns - 1) * source.step);
                out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = source.sampleCubic(lo, la);
            }
        }
        return true;
    }

    bool loadWeek(const GfsData::Run&, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) { return loadPeriod(true, hour, want, out, error); }
    bool loadMonth(const GfsData::Run&, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) { return loadPeriod(false, hour, want, out, error); }
}

void Ec46Loader::registerLoader() {
    GfsData::setLoader("EC46", loadWeek);
    GfsData::setLoader("SEAS5", loadMonth);
}
