// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityHurdat.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {
    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r");
        return a == string::npos ? string{} : s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    }

    vector<string> split(const string& line) {
        vector<string> parts;
        size_t start = 0;
        while (true) {
            const auto comma = line.find(',', start);
            parts.push_back(trim(line.substr(start, comma == string::npos ? string::npos : comma - start)));
            if (comma == string::npos) {
                break;
            }
            start = comma + 1;
        }
        return parts;
    }
}

bool UtilityHurdat::parseCoordinate(const string& field, double& degrees) {
    if (field.size() < 3) {
        return false;
    }
    const char hemisphere = field.back();
    if (hemisphere != 'N' && hemisphere != 'S' && hemisphere != 'E' && hemisphere != 'W') {
        return false;
    }
    char * end = nullptr;
    const double value = std::strtod(field.c_str(), &end);
    if (end == field.c_str()) {
        return false;
    }
    degrees = (hemisphere == 'S' || hemisphere == 'W') ? -value : value;
    return true;
}

vector<UtilityHurdat::Track> UtilityHurdat::parse(const string& text) {
    vector<Track> tracks;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        if (line.size() > 8 && std::isalpha(static_cast<unsigned char>(line[0])) && std::isalpha(static_cast<unsigned char>(line[1]))) {
            const auto parts = split(line);
            if (parts.size() >= 2 && parts[0].size() == 8) {
                Track t;
                t.id = parts[0];
                t.name = parts[1];
                t.year = std::atoi(parts[0].substr(4, 4).c_str());
                tracks.push_back(std::move(t));
            }
            continue;
        }
        if (tracks.empty()) {
            continue;
        }
        const auto parts = split(line);
        if (parts.size() < 8 || parts[0].size() != 8 || parts[1].size() != 4) {
            continue;
        }
        Point p;
        if (!parseCoordinate(parts[4], p.lat) || !parseCoordinate(parts[5], p.lon)) {
            continue;
        }
        p.time = parts[0] + parts[1].substr(0, 2);
        p.status = parts[3];
        p.wind = std::max(0, std::atoi(parts[6].c_str()));
        p.pressure = std::atoi(parts[7].c_str());
        if (p.pressure < 800 || p.pressure > 1100) {
            p.pressure = 0;
        }
        auto& t = tracks.back();
        t.peakWind = std::max(t.peakWind, p.wind);
        if (p.pressure > 0 && (t.minPressure == 0 || p.pressure < t.minPressure)) {
            t.minPressure = p.pressure;
        }
        if ((p.status == "TS" || p.status == "SS" || p.status == "HU") && p.wind >= 34) {
            t.stormStrength = true;
        }
        t.points.push_back(std::move(p));
    }
    return tracks;
}

namespace {
    // days since 1970-01-01 of a civil date
    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    // hours since 1970-01-01 of "yyyymmddhh"; -1 when the text is not one
    double hoursOf(const std::string& time) {
        if (time.size() < 10) {
            return -1.0;
        }
        return static_cast<double>(daysFromCivil(std::atoi(time.substr(0, 4).c_str()), std::atoi(time.substr(4, 2).c_str()), std::atoi(time.substr(6, 2).c_str()))) * 24.0 +
            std::atoi(time.substr(8, 2).c_str());
    }

    double kilometersBetween(double lat1, double lon1, double lat2, double lon2) {
        const double rad = 3.14159265358979 / 180.0;
        const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
        return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
    }
}

double UtilityHurdat::ace(const Track& t) {
    double sum = 0.0;
    for (const auto& p : t.points) {
        const std::string hour = p.time.size() >= 10 ? p.time.substr(8, 2) : std::string{};
        if (p.wind >= 34 && (p.status == "TS" || p.status == "SS" || p.status == "HU") && (hour == "00" || hour == "06" || hour == "12" || hour == "18")) {   // tropical or subtropical storm stage only
            sum += static_cast<double>(p.wind) * p.wind;
        }
    }
    return sum / 10000.0;
}

double UtilityHurdat::lengthKm(const Track& t) {
    double km = 0.0;
    for (size_t i = 1; i < t.points.size(); i++) {
        km += kilometersBetween(t.points[i - 1].lat, t.points[i - 1].lon, t.points[i].lat, t.points[i].lon);
    }
    return km;
}

double UtilityHurdat::durationDays(const Track& t) {
    if (t.points.size() < 2) {
        return 0.0;
    }
    const double first = hoursOf(t.points.front().time), last = hoursOf(t.points.back().time);
    return first < 0.0 || last < first ? 0.0 : (last - first) / 24.0;
}

std::vector<std::string> UtilityHurdat::sortNames() {
    return {"Strongest wind first", "Lowest pressure first", "Highest ACE first", "Longest track first", "Longest-lived first", "Newest first", "Oldest first"};
}

double UtilityHurdat::sortKey(const Track& t, Sort sort) {
    switch (sort) {
        case Sort::LowestPressure: return t.minPressure > 0 ? -static_cast<double>(t.minPressure) : -1.0e9;   // storms with no pressure given come last
        case Sort::Ace: return ace(t);
        case Sort::Longest: return lengthKm(t);
        case Sort::LongestLived: return durationDays(t);
        case Sort::Newest: return t.points.empty() ? static_cast<double>(t.year) * 1.0e6 : hoursOf(t.points.front().time);
        case Sort::Oldest: return t.points.empty() ? -static_cast<double>(t.year) * 1.0e6 : -hoursOf(t.points.front().time);
        case Sort::Strongest: break;
    }
    return static_cast<double>(t.peakWind);
}

std::string UtilityHurdat::sortNote(const Track& t, Sort sort) {
    char text[48];
    switch (sort) {
        case Sort::Ace:
            std::snprintf(text, sizeof text, "ACE %.1f", ace(t));
            return text;
        case Sort::Longest:
            std::snprintf(text, sizeof text, "%d km", static_cast<int>(std::lround(lengthKm(t))));
            return text;
        case Sort::LongestLived:
            std::snprintf(text, sizeof text, "%.1f days", durationDays(t));
            return text;
        default:
            return {};
    }
}
