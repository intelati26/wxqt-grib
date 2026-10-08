// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYDATE_H
#define UTILITYDATE_H

#include <string>

// How dates are written on the screens: ISO 8601, year first (2026-10-08), so that no reader has to guess whether 1/10 is 1 October or 10 January, and so that the text sorts. A time
// is "2026-10-08 14:02Z". A day with no year (the days of a seasonal chart, which stand for any year) is month-day, "10-08". Pure arithmetic, no Qt: for Qt dates use the format
// strings "yyyy-MM-dd" and "yyyy-MM-dd HH:mm".
namespace UtilityDate {
    struct Civil {
        int year{1970};
        int month{1};
        int day{1};
        long secondsOfDay{0};   // 0 .. 86399
    };
    Civil civil(long unixSeconds);                          // the UTC calendar date and time of day
    std::string iso(int year, int month, int day);          // "2026-10-08"
    std::string iso(long unixSeconds);                      // the UTC date of an instant: "2026-10-08"
    std::string isoMinute(long unixSeconds);                // "2026-10-08 14:02Z"
    std::string monthDay(int month, int day);               // "10-08"
    std::string dayOfYear(int day);                         // the day (1 to 365) of a non-leap year: 281 -> "10-08"
}

#endif  // UTILITYDATE_H
