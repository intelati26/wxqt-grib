// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTDAILY_H
#define DROUGHTDAILY_H

#include <string>
#include <vector>
#include <QDate>

// How far each day's temperature was from normal over an area, from NOAA's nClimGrid-Daily area averages (NCEI: one CSV a month for the states, the weather service's forecast office regions and the
// contiguous U.S., a value for every day). The normal of a day is the mean of that calendar day over 1991-2020, worked out once from the files of those years and kept (drought/daily in the data
// folder); the days are then a trailing mean over a number of days to smooth them. A month that is final is kept as it came; the current one is asked for again.
namespace DroughtDaily {
    struct Key {
        std::string kind;   // "ste", "wfo" or "cns"
        std::string code;   // the office's three letters, "110" for the country
        std::string name;   // a state's name (the files name states, not number them the way the Census does)
    };
    struct Point {
        QDate date;
        double departure{0};   // degrees C
    };
    // the area as the files have it: the country, a state (a county has its state's), an office; false for the areas they have nothing for
    bool keyFor(const std::string& areaId, const std::string& areaName, const std::string& stateName, Key& out);
    // The departure from normal of the daily maximum (or minimum) temperature for every day from `from` to `to`, smoothed by a mean of the `smooth` days up to each (1: the days as they are). Slow the
    // first time (the normals are made from 360 months of files).
    bool departures(const Key& key, bool maximum, const QDate& from, const QDate& to, int smooth, std::vector<Point>& out, std::string& error);
}

#endif  // DROUGHTDAILY_H
