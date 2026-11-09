// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef AIRNOWLOADER_H
#define AIRNOWLOADER_H

#include <vector>
#include <QDate>

// EPA's AirNow: the air quality of the reporting areas (about 1300 cities and their surroundings), from the public file of its forecasts and observations
// (https://files.airnowtech.org/airnow/today/reportingarea.dat, refreshed through the day). The forecasts give a category for the day (Good, Moderate ...), for up to about five days; the observations
// of today give the index itself.
namespace AirNow {
    struct Point {
        double lat{0}, lon{0};
        int category{0};      // 0 Good, 1 Moderate, 2 Unhealthy for Sensitive Groups, 3 Unhealthy, 4 Very Unhealthy, 5 Hazardous
        double aqi{-1.0};     // the index where the file has it (the observations of today); -1: only the category is known
    };
    // The worst of the day's forecasts and (for today) observations of each reporting area, for the date given (the local date of the place: the file's own). Empty when the file could not be read or has
    // nothing for the date. The file is kept for twenty minutes.
    std::vector<Point> points(const QDate& date);
    // The category of an index (0 up to 50 Good ...)
    int categoryOf(double aqi);
}

#endif  // AIRNOWLOADER_H
