// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/UtilityTornado.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <regex>
#include <sstream>
#include "obs/UtilityMetarCache.h"

namespace {
    double number(const string& s, double fallback = 0.0) {
        if (s.empty()) {
            return fallback;
        }
        char * end = nullptr;
        const double v = std::strtod(s.c_str(), &end);
        return end == s.c_str() ? fallback : v;
    }

    int integer(const string& s, int fallback = 0) {
        return static_cast<int>(std::lround(number(s, fallback)));
    }
}

std::string UtilityTornado::isoDate(int year, int month, int day) {
    char text[16];
    std::snprintf(text, sizeof text, "%04d-%02d-%02d", year, month, day);
    return text;
}

int UtilityTornado::dayOfYear(int year, int month, int day) {
    static const int before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return before[month - 1] + day + (leap && month > 2 ? 1 : 0);
}

vector<UtilityTornado::Tornado> UtilityTornado::parse(const string& csv) {
    vector<Tornado> all;
    std::istringstream in{csv};
    string line;
    std::map<string, size_t> column;
    while (std::getline(in, line)) {
        const auto f = UtilityMetarCache::splitCsv(line);
        if (column.empty()) {
            for (size_t i = 0; i < f.size(); i++) {
                column[f[i]] = i;
            }
            if (!column.contains("yr") || !column.contains("slat") || !column.contains("mag")) {
                return {};   // not the file
            }
            continue;
        }
        const auto get = [&] (const char * name) -> string {
            const auto it = column.find(name);
            return it == column.end() || it->second >= f.size() ? string{} : f[it->second];
        };
        Tornado t;
        t.id = get("om");
        t.year = integer(get("yr"));
        t.month = integer(get("mo"));
        t.day = integer(get("dy"));
        t.time = get("time");
        t.timeZone = integer(get("tz"), 3);
        t.state = get("st");
        t.mag = integer(get("mag"), -9);
        t.injuries = integer(get("inj"));
        t.fatalities = integer(get("fat"));
        t.loss = number(get("loss"));
        t.startLat = number(get("slat"));
        t.startLon = number(get("slon"));
        t.endLat = number(get("elat"));
        t.endLon = number(get("elon"));
        t.length = number(get("len"));
        t.width = number(get("wid"));
        t.states = integer(get("ns"), 1);
        t.segment = integer(get("sg"), 1);
        t.dayOfYear = dayOfYear(t.year, t.month, t.day);
        if (t.year < 1900 || t.dayOfYear == 0) {
            continue;
        }
        all.push_back(std::move(t));
    }
    return all;
}

void UtilityTornado::nextDay(int& year, int& month, int& day) {
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    const int length = lengths[std::clamp(month, 1, 12) - 1] + (leap && month == 2 ? 1 : 0);
    if (++day > length) {
        day = 1;
        if (++month > 12) {
            month = 1;
            year++;
        }
    }
}

vector<UtilityTornado::Tornado> UtilityTornado::parseDailyReport(const string& csv, int year, int month, int day) {
    vector<Tornado> out;
    std::istringstream in{csv};
    string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            first = false;
            if (line.rfind("Time", 0) != 0) {
                return {};   // not a report file
            }
            continue;
        }
        const auto f = UtilityMetarCache::splitCsv(line);
        if (f.size() < 7) {
            continue;
        }
        Tornado t;
        const int hhmm = integer(f[0], -1);
        if (hhmm < 0 || hhmm > 2359) {
            continue;
        }
        int y = year, m = month, d = day;
        if (hhmm < 1200) {
            nextDay(y, m, d);
        }
        t.year = y;
        t.month = m;
        t.day = d;
        t.dayOfYear = dayOfYear(y, m, d);
        char buffer[16];
        std::snprintf(buffer, sizeof buffer, "%02d:%02d:00", hhmm / 100, hhmm % 100);
        t.time = buffer;
        t.timeZone = 9;   // UTC
        t.state = f[4];
        // "UNK", or "EF1" / "F1" / "1"
        const string rating = f[1];
        const auto digit = rating.find_first_of("012345");
        t.mag = digit != string::npos && rating.find("UNK") == string::npos ? rating[digit] - '0' : -9;
        t.startLat = number(f[5]);
        t.startLon = number(f[6]);
        if (t.startLat == 0.0 && t.startLon == 0.0) {
            continue;
        }
        t.id = "prelim-" + std::to_string(year) + (month < 10 ? "0" : "") + std::to_string(month) + (day < 10 ? "0" : "") + std::to_string(day) + "-" + std::to_string(out.size() + 1);
        t.preliminary = true;
        out.push_back(std::move(t));
    }
    return out;
}

bool UtilityTornado::passesRating(const Tornado& t, int level) {
    if (level <= 0) {
        return true;
    }
    if (level <= 4) {
        return t.mag >= level || (t.preliminary && t.mag < 0);
    }
    return level == 5 ? t.mag == 5 : t.mag < 0 && !t.preliminary;
}

string UtilityTornado::ratingOf(int mag, int year) {
    if (mag < 0) {
        return "unrated";
    }
    return string{year >= 2007 ? "EF" : "F"} + std::to_string(mag);
}

string UtilityTornado::rating(const Tornado& t) {
    return ratingOf(t.mag, t.year);
}

double UtilityTornado::kilometers(double lat1, double lon1, double lat2, double lon2) {
    const double rad = 3.14159265358979 / 180.0;
    const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
    return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
}

double UtilityTornado::distanceToTrack(const Tornado& t, double lat, double lon) {
    double best = kilometers(lat, lon, t.startLat, t.startLon);
    if (!t.hasEnd()) {
        return best;
    }
    best = std::min(best, kilometers(lat, lon, t.endLat, t.endLon));
    // the nearest point of the straight track, on a flat plane about the point (tracks are a few tens of kilometres at most)
    const double cosLat = std::cos(lat * 3.14159265358979 / 180.0);
    const double ax = (t.startLon - lon) * cosLat * 111.195;
    const double ay = (t.startLat - lat) * 111.195;
    const double bx = (t.endLon - lon) * cosLat * 111.195;
    const double by = (t.endLat - lat) * 111.195;
    const double dx = bx - ax;
    const double dy = by - ay;
    const double length2 = dx * dx + dy * dy;
    const double f = length2 > 0.0 ? std::clamp(-(ax * dx + ay * dy) / length2, 0.0, 1.0) : 0.0;
    return std::min(best, std::hypot(ax + dx * f, ay + dy * f));
}

string UtilityTornado::newestFile(const string& page) {
    static const std::regex re{R"re(1950-(\d{4})_actual_tornadoes\.csv)re"};
    string best;
    int year = 0;
    for (std::sregex_iterator it{page.begin(), page.end(), re}, end; it != end; ++it) {
        const int y = std::atoi((*it)[1].str().c_str());
        if (y > year) {
            year = y;
            best = (*it)[0];
        }
    }
    return best;
}

double UtilityTornado::value(const Tornado& t, Metric metric) {
    return metric == Metric::Count ? 1.0 : metric == Metric::Deaths ? t.fatalities : t.injuries;
}

vector<std::pair<int, double>> UtilityTornado::buckets(const vector<const Tornado *>& list, Group group, Metric metric) {
    std::map<int, double> sums;
    for (const auto * t : list) {
        if (!t->counts()) {
            continue;
        }
        int key = 0;
        switch (group) {
            case Group::DayOfYear: key = t->dayOfYear; break;
            case Group::Heatmap:
            case Group::Week: key = (t->dayOfYear - 1) / 7 + 1; break;
            case Group::Month: key = t->month; break;
            case Group::Year: key = t->year; break;
            case Group::Decade: key = t->year / 10 * 10; break;
        }
        sums[key] += value(*t, metric);
    }
    return {sums.begin(), sums.end()};
}
