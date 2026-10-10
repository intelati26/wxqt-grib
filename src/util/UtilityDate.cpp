// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/UtilityDate.h"
#include <algorithm>
#include <cstdio>

UtilityDate::Civil UtilityDate::civil(long seconds) {
    // days since 1970-01-01 (floored for times before it), then the civil date from them (Howard Hinnant's algorithm)
    long days = seconds / 86400L;
    long rest = seconds % 86400L;
    if (rest < 0) {
        rest += 86400L;
        days -= 1;
    }
    days += 719468;
    const long era = (days >= 0 ? days : days - 146096) / 146097;
    const long doe = days - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    Civil c;
    c.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    c.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    c.year = static_cast<int>(yoe + era * 400 + (c.month <= 2 ? 1 : 0));
    c.secondsOfDay = rest;
    return c;
}

std::string UtilityDate::iso(int year, int month, int day) {
    char text[24];
    std::snprintf(text, sizeof text, "%04d-%02d-%02d", year, month, day);
    return text;
}

std::string UtilityDate::iso(long seconds) {
    const auto c = civil(seconds);
    return iso(c.year, c.month, c.day);
}

std::string UtilityDate::isoMinute(long seconds) {
    const auto c = civil(seconds);
    char text[40];
    std::snprintf(text, sizeof text, "%s %02ld:%02ldZ", iso(c.year, c.month, c.day).c_str(), c.secondsOfDay / 3600, c.secondsOfDay % 3600 / 60);
    return text;
}

std::string UtilityDate::monthDay(int month, int day) {
    char text[16];
    std::snprintf(text, sizeof text, "%02d-%02d", month, day);
    return text;
}

std::string UtilityDate::dayOfYear(int day) {
    static const int length[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    day = std::clamp(day, 1, 365);
    int month = 0;
    while (month < 11 && day > length[month]) {
        day -= length[month];
        month++;
    }
    return monthDay(month + 1, day);
}
