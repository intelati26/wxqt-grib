// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilitySeason.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>

namespace {
    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r");
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    }

    vector<string> splitComma(const string& line) {
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

    void note(UtilitySeason::Storm& storm, const string& time, const string& status, int wind, int pressure, int hour, const std::array<std::array<int, 4>, 3> * radii = nullptr) {
        if (storm.first.empty()) {
            storm.first = time;
        }
        storm.last = time;
        if (wind > storm.peakWind) {
            storm.peakWind = wind;
        }
        if (pressure > 800 && pressure < 1100 && (storm.minPressure == 0 || pressure < storm.minPressure)) {
            storm.minPressure = pressure;
        }
        if ((status == "TS" || status == "SS" || status == "HU") && wind >= 34) {
            storm.stormStrength = true;
        }
        const double ace = UtilitySeason::recordAce(hour, status, wind);
        storm.ace += ace;
        if (radii != nullptr) {   // the wind radii of the record are known (HURDAT2 has -999 before 2004)
            storm.hasRadii = true;
            const double ike = UtilitySeason::recordIke(hour, status, wind, *radii);
            storm.tike += ike;
            if (ike > 0.0) {
                const int day = UtilitySeason::dayOfYear(time.substr(0, 8));
                if (day > 0) {
                    if (!storm.dailyTike.empty() && storm.dailyTike.back().first == day) {
                        storm.dailyTike.back().second += ike;
                    } else {
                        storm.dailyTike.emplace_back(day, ike);
                    }
                }
            }
        }
        if (ace > 0.0) {
            const int day = UtilitySeason::dayOfYear(time.substr(0, 8));
            if (day > 0) {
                if (!storm.daily.empty() && storm.daily.back().first == day) {
                    storm.daily.back().second += ace;
                } else {
                    storm.daily.emplace_back(day, ace);
                }
            }
        }
    }
}

double UtilitySeason::recordAce(int hourUtc, const string& status, int windKt) {
    if (hourUtc % 6 != 0 || windKt < 34 || !(status == "TS" || status == "SS" || status == "HU")) {
        return 0.0;
    }
    return static_cast<double>(windKt) * windKt / 10000.0;
}

double UtilitySeason::recordIke(int hourUtc, const string& status, int windKt, const std::array<std::array<int, 4>, 3>& radii) {
    if (hourUtc % 6 != 0 || windKt < 34 || !(status == "TS" || status == "SS" || status == "HU")) {
        return 0.0;
    }
    // the area inside each radius, nm2 (four quarter circles)
    double area[3] = {0.0, 0.0, 0.0};
    for (int k = 0; k < 3; k++) {
        for (int q = 0; q < 4; q++) {
            const double r = std::max(0, radii[static_cast<size_t>(k)][static_cast<size_t>(q)]);
            area[k] += 3.14159265358979 * r * r / 4.0;
        }
    }
    // a 50 kt radius cannot be wider than the 34 kt one, nor the 64 kt wider than the 50
    area[1] = std::min(area[1], area[0]);
    area[2] = std::min(area[2], area[1]);
    const double vmax = static_cast<double>(windKt);
    const auto mean = [vmax] (double lo, double hi) { return (lo + std::min(hi, vmax)) / 2.0; };   // knots
    const double band34 = area[0] - area[1];
    const double band50 = area[1] - area[2];
    const double band64 = area[2];
    const double knotsToMs = 0.514444;
    const double nm2ToM2 = 1852.0 * 1852.0;
    double joules = 0.0;
    joules += band34 * nm2ToM2 * std::pow(mean(34.0, 50.0) * knotsToMs, 2.0);
    joules += band50 * nm2ToM2 * std::pow(mean(50.0, 64.0) * knotsToMs, 2.0);
    joules += band64 * nm2ToM2 * std::pow(((64.0 + vmax) / 2.0) * knotsToMs, 2.0);
    return 0.5 * 1.0 * joules * 1.0 / 1.0e12;   // rho 1 kg/m3, 1 m deep, terajoules
}

vector<UtilitySeason::Storm> UtilitySeason::parseHurdat2(const string& text) {
    vector<Storm> storms;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        if (line.size() > 8 && std::isalpha(static_cast<unsigned char>(line[0])) && std::isalpha(static_cast<unsigned char>(line[1]))) {
            // header: AL092011,              IRENE,     39,
            const auto parts = splitComma(line);
            if (parts.size() >= 2 && parts[0].size() == 8) {
                Storm storm;
                storm.id = parts[0];
                storm.name = parts[1];
                storm.year = std::atoi(parts[0].substr(4, 4).c_str());
                storms.push_back(storm);
            }
            continue;
        }
        if (storms.empty()) {
            continue;
        }
        // data: 20110828, 0935, L, TS, 39.4N, 74.4W, 60, 959, ...
        const auto parts = splitComma(line);
        if (parts.size() < 8 || parts[0].size() != 8 || parts[1].size() != 4) {
            continue;
        }
        const int wind = std::atoi(parts[6].c_str());
        const int pressure = std::atoi(parts[7].c_str());
        const int hhmm = std::atoi(parts[1].c_str());
        // the synoptic times only count for ACE: 00, 06, 12, 18 UTC on the hour (the file also holds landfall and peak records between)
        const int hourForAce = hhmm % 100 == 0 ? hhmm / 100 : 1;
        // the wind radii, nm: 34 kt NE SE SW NW, 50 kt, 64 kt (columns 9 to 20); -999 where the file has none
        std::array<std::array<int, 4>, 3> radii{};
        bool known = parts.size() >= 20;
        for (size_t k = 0; k < 3 && known; k++) {
            for (size_t q = 0; q < 4; q++) {
                const int v = std::atoi(parts[8 + k * 4 + q].c_str());
                if (v <= -990) {
                    known = false;
                    break;
                }
                radii[k][q] = v;
            }
        }
        note(storms.back(), parts[0] + parts[1].substr(0, 2), parts[3], wind > 0 ? wind : 0, pressure, hourForAce, known ? &radii : nullptr);
    }
    return storms;
}

UtilitySeason::Storm UtilitySeason::fromBestTrack(const vector<UtilityAtcf::Fix>& best, const string& id) {
    Storm storm;
    string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    storm.id = upper;
    storm.year = upper.size() >= 8 ? std::atoi(upper.substr(4, 4).c_str()) : 0;
    for (const auto& f : best) {
        if (!f.name.empty() && f.name != "INVEST" && f.name != "NONAME") {
            storm.name = f.name;
        }
        const int hour = f.time.size() == 10 ? std::atoi(f.time.substr(8, 2).c_str()) : 1;
        note(storm, f.time, f.status, f.wind > 0 ? f.wind : 0, f.pressure, hour, &f.radii);
    }
    if (storm.name.empty()) {
        storm.name = "UNNAMED";
    }
    return storm;
}

int UtilitySeason::dayOfYear(const string& date) {
    if (date.size() != 8) {
        return 0;
    }
    const int y = std::atoi(date.substr(0, 4).c_str());
    const int m = std::atoi(date.substr(4, 2).c_str());
    const int d = std::atoi(date.substr(6, 2).c_str());
    if (y < 1800 || m < 1 || m > 12 || d < 1 || d > 31) {
        return 0;
    }
    static const int before[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return before[m - 1] + d + (leap && m > 2 ? 1 : 0);
}

vector<double> UtilitySeason::cumulativeByDay(const vector<Storm>& storms, int year, Metric metric) {
    vector<double> perDay(367, 0.0);
    for (const auto& s : storms) {
        if (s.year != year) {
            continue;
        }
        for (const auto& [day, ace] : metric == Metric::Ace ? s.daily : s.dailyTike) {
            perDay[static_cast<size_t>(std::clamp(day, 1, 366))] += ace;
        }
    }
    for (size_t d = 1; d < perDay.size(); d++) {
        perDay[d] += perDay[d - 1];
    }
    return perDay;
}

UtilitySeason::Climatology UtilitySeason::climatology(const vector<Storm>& storms, int firstYear, int lastYear, Metric metric) {
    Climatology c;
    c.mean.assign(367, 0.0);
    c.lowest.assign(367, 1e18);
    c.highest.assign(367, 0.0);
    for (int year = firstYear; year <= lastYear; year++) {
        const auto cumulative = cumulativeByDay(storms, year, metric);
        for (size_t d = 0; d < 367; d++) {
            c.mean[d] += cumulative[d];
            c.lowest[d] = std::min(c.lowest[d], cumulative[d]);
            c.highest[d] = std::max(c.highest[d], cumulative[d]);
        }
        c.years++;
    }
    for (auto& v : c.mean) {
        v = c.years > 0 ? v / c.years : 0.0;
    }
    if (c.years == 0) {
        c.lowest.assign(367, 0.0);
    }
    return c;
}

vector<UtilitySeason::Season> UtilitySeason::seasons(const vector<Storm>& storms) {
    std::map<int, Season> byYear;
    for (const auto& s : storms) {
        auto& season = byYear[s.year];
        season.year = s.year;
        season.cyclones++;
        season.named += s.stormStrength ? 1 : 0;
        season.hurricanes += s.peakWind >= 64 && s.stormStrength ? 1 : 0;
        season.major += s.peakWind >= 96 && s.stormStrength ? 1 : 0;
        season.ace += s.ace;
        season.tike += s.tike;
        season.radiiStorms += s.hasRadii ? 1 : 0;
    }
    vector<Season> out;
    for (const auto& [year, season] : byYear) {
        out.push_back(season);
    }
    return out;
}

string UtilitySeason::csv(const vector<Storm>& storms) {
    std::ostringstream out;
    out << std::setprecision(12);
    for (const auto& s : storms) {
        out << s.id << ',' << s.name << ',' << s.year << ',' << s.first << ',' << s.last << ',' << s.peakWind << ',' << s.minPressure << ',' << s.ace << ','
            << (s.stormStrength ? 1 : 0) << ',';
        for (size_t i = 0; i < s.daily.size(); i++) {
            out << (i == 0 ? "" : "|") << s.daily[i].first << ':' << s.daily[i].second;
        }
        out << ',' << s.tike << ',' << (s.hasRadii ? 1 : 0) << ',';
        for (size_t i = 0; i < s.dailyTike.size(); i++) {
            out << (i == 0 ? "" : "|") << s.dailyTike[i].first << ':' << s.dailyTike[i].second;
        }
        out << '\n';
    }
    return out.str();
}

vector<UtilitySeason::Storm> UtilitySeason::fromCsv(const string& text) {
    vector<Storm> storms;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        const auto p = splitComma(line);
        if (p.size() != 13) {   // the thirteen column form (the daily ACE, then the TIKE, whether there were radii, the daily TIKE)
            continue;
        }
        Storm s;
        s.id = p[0];
        s.name = p[1];
        s.year = std::atoi(p[2].c_str());
        s.first = p[3];
        s.last = p[4];
        s.peakWind = std::atoi(p[5].c_str());
        s.minPressure = std::atoi(p[6].c_str());
        s.ace = std::strtod(p[7].c_str(), nullptr);
        s.stormStrength = p[8] == "1";
        const auto readDays = [] (const string& text, vector<std::pair<int, double>>& into) {
            std::istringstream days{text};
            string item;
            while (std::getline(days, item, '|')) {
                const auto colon = item.find(':');
                if (colon != string::npos) {
                    into.emplace_back(std::atoi(item.c_str()), std::strtod(item.c_str() + colon + 1, nullptr));
                }
            }
        };
        readDays(p[9], s.daily);
        s.tike = std::strtod(p[10].c_str(), nullptr);
        s.hasRadii = p[11] == "1";
        readDays(p[12], s.dailyTike);
        storms.push_back(s);
    }
    return storms;
}

string UtilitySeason::categoryName(int wind) {
    return UtilityAtcf::categoryName(UtilityAtcf::categoryOf(wind));
}

string UtilitySeason::newestHurdatFile(const string& listing, const string& prefix) {
    // hurdat2-1851-2025-092326.txt: the date is month day year, with a two or four digit year (02272026 is 27 February 2026)
    const std::regex re{prefix + R"re(-(\d{4})-(\d{2})(\d{2})(\d{2}|\d{4})\.txt)re"};
    string best;
    long bestKey = -1;
    for (std::sregex_iterator it{listing.begin(), listing.end(), re}, end; it != end; ++it) {
        const long through = std::atol((*it)[1].str().c_str());
        int year = std::atoi((*it)[4].str().c_str());
        if (year < 100) {
            year += 2000;
        }
        const long key = (through * 10000L + year) * 10000L + std::atol((*it)[2].str().c_str()) * 100L + std::atol((*it)[3].str().c_str());
        if (key > bestKey) {
            bestKey = key;
            best = (*it)[0];
        }
    }
    return best;
}
