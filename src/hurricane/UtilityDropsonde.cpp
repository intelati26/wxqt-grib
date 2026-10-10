// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityDropsonde.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace {
    const double none = UtilityDropsonde::missing;

    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    }

    bool digits(const string& s, size_t from, size_t count) {
        if (s.size() < from + count) {
            return false;
        }
        for (size_t i = from; i < from + count; i++) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
                return false;
            }
        }
        return true;
    }

    int toInt(const string& s) {
        return std::atoi(s.c_str());
    }

    // whole millibars with the thousands left out: 009 -> 1009
    double pressureOf(const string& ppp) {
        if (!digits(ppp, 0, 3)) {
            return none;
        }
        const int p = toInt(ppp);
        return p < 100 ? p + 1000 : p;
    }

    // TTTDD: the temperature in tenths of a degree, positive when the tenths digit is even, negative when odd; the dew point depression
    // 00-50 in tenths of a degree, 56-99 in whole degrees plus 50 (so 56 is 6 C), 51-55 not used
    void temperatureGroup(const string& g, double& temperature, double& dewPoint) {
        temperature = none;
        dewPoint = none;
        if (g.size() != 5 || !digits(g, 0, 3)) {
            return;
        }
        const int ttt = toInt(g.substr(0, 3));
        temperature = (ttt % 2 == 0 ? 1.0 : -1.0) * ttt / 10.0;
        if (digits(g, 3, 2)) {
            const int dd = toInt(g.substr(3, 2));
            double depression = none;
            if (dd <= 50) {
                depression = dd / 10.0;
            } else if (dd >= 56) {
                depression = dd - 50.0;
            }
            if (depression > none + 1.0) {
                dewPoint = temperature - depression;
            }
        }
    }

    // dddff: the direction to the nearest 5 degrees with the hundreds of the speed in the units digit (1 or 6 means 100 kt more, 2 or 7 means 200 ...)
    void windGroup(const string& g, double& direction, double& speed) {
        direction = none;
        speed = none;
        if (g.size() != 5 || !digits(g, 0, 5)) {
            return;
        }
        int ddd = toInt(g.substr(0, 3));
        int ff = toInt(g.substr(3, 2));
        const int hundreds = ddd % 5;
        ddd -= hundreds;
        ff += 100 * hundreds;
        direction = ddd;
        speed = ff;
    }

    // the height of a mandatory level from its three digits (WMO FM 35 / FM 37, section 2)
    double heightOf(int level, int hhh) {
        switch (level) {
            case 1000: return hhh >= 500 ? -(hhh - 500) : hhh;        // metres, 500 added for a negative height
            case 925: return hhh;                                        // metres
            case 850: return 1000 + hhh;                                 // metres, the thousand left out
            case 700: return hhh > 500 ? 2000 + hhh : 3000 + hhh;      // metres, the 2000 or 3000 left out
            case 500: case 400: return hhh * 10.0;                       // decametres
            case 300: return (hhh < 500 ? hhh + 1000 : hhh) * 10.0;     // decametres, 1000 added when small
            default: return (hhh + 1000) * 10.0;                         // 250 hPa and above: decametres, 1000 added
        }
    }

    int mandatoryLevel(const string& pp) {
        static const std::map<string, int> levels{{"00", 1000}, {"92", 925}, {"85", 850}, {"70", 700}, {"50", 500}, {"40", 400}, {"30", 300}, {"25", 250}, {"20", 200}, {"15", 150}, {"10", 100}};
        const auto found = levels.find(pp);
        return found == levels.end() ? 0 : found->second;
    }

    bool isKeyword(const string& t) {
        static const std::set<string> keywords{"XXAA", "XXBB", "21212", "31313", "41414", "51515", "52525", "53535", "54545", "55555", "56565", "57575", "58585", "59595", "61616", "62626"};
        return keywords.contains(t);
    }

    using Levels = std::map<long, UtilityDropsonde::Level>;   // key: pressure in tenths of a mb

    UtilityDropsonde::Level& at(Levels& levels, double pressure) {
        auto& level = levels[std::lround(pressure * 10.0)];
        level.pressure = pressure;
        return level;
    }

    // "2813N08465W": hundredths of a degree
    bool position(const string& lat, const string& lon, double& outLat, double& outLon) {
        if (lat.size() != 5 || lon.size() != 6 || !digits(lat, 0, 4) || !digits(lon, 0, 5)) {
            return false;
        }
        outLat = toInt(lat.substr(0, 4)) / 100.0 * (lat[4] == 'S' ? -1.0 : 1.0);
        outLon = toInt(lon.substr(0, 5)) / 100.0 * (lon[5] == 'W' ? -1.0 : 1.0);
        return true;
    }
}

UtilityDropsonde::Drop UtilityDropsonde::parse(const string& text, const string& fileStamp) {
    Drop drop;
    std::istringstream stream{text};
    vector<string> tokens;
    string word;
    while (stream >> word) {
        if (!word.empty() && word.back() == '=') {
            word.pop_back();
        }
        if (!word.empty()) {
            tokens.push_back(word);
        }
    }
    Levels levels;
    int day = 0;
    int obsHour = -1;
    int launchHour = -1;
    int launchMinute = 0;
    bool haveA = false;
    const size_t n = tokens.size();
    const auto token = [&] (size_t i) -> const string& {
        static const string empty;
        return i < n ? tokens[i] : empty;
    };
    for (size_t i = 0; i < n; i++) {
        const auto& t = tokens[i];
        if (t == "XXAA" && !haveA) {
            const auto& group = token(i + 1);   // YYGGId
            if (!digits(group, 0, 5)) {
                continue;
            }
            const int yy = toInt(group.substr(0, 2));
            drop.windInKnots = yy >= 50;
            day = yy >= 50 ? yy - 50 : yy;
            obsHour = toInt(group.substr(2, 2));
            // Id: the highest standard level that has a wind group: 0 = 1000 hPa, 9 = 925, 8 = 850, 7 = 700, 5 = 500, 4 = 400, 3 = 300, 2 = 200, 1 = 100; / = no winds
            static const std::map<char, int> windTop{{'0', 1000}, {'9', 925}, {'8', 850}, {'7', 700}, {'5', 500}, {'4', 400}, {'3', 300}, {'2', 200}, {'1', 100}};
            const auto top = windTop.find(group[4]);
            const int windDownTo = top == windTop.end() ? 100000 : top->second;   // winds are given for levels at or below (pressure at or above) this
            // 99LaLaLa QcLoLoLoLo MMMULaULo
            const auto& la = token(i + 2);
            const auto& lo = token(i + 3);
            if (la.size() == 5 && la.compare(0, 2, "99") == 0 && digits(la, 2, 3) && digits(lo, 0, 5)) {
                const int quadrant = lo[0] - '0';
                const double lat = toInt(la.substr(2, 3)) / 10.0;
                const double lon = toInt(lo.substr(1, 4)) / 10.0;
                drop.lat = (quadrant == 3 || quadrant == 5) ? -lat : lat;
                drop.lon = (quadrant == 5 || quadrant == 7) ? -lon : lon;
            }
            size_t j = i + 5;
            // the surface: 99PPP TTTDD dddff
            if (token(j).size() == 5 && token(j).compare(0, 2, "99") == 0) {
                drop.surfacePressure = pressureOf(token(j).substr(2, 3));
                auto& s = at(levels, drop.surfacePressure);
                s.height = 0.0;
                temperatureGroup(token(j + 1), s.temperature, s.dewPoint);
                windGroup(token(j + 2), s.windDirection, s.windSpeed);
                j += 3;
                if (windDownTo == 100000) {
                    j -= 1;   // no winds at all: the surface group has two parts
                }
            }
            // the mandatory levels: PPhhh TTTDD dddff, then 88PPP (tropopause) and 77PPP (maximum wind), 88999 / 77999 for none
            while (j < n && !isKeyword(token(j))) {
                const auto& g = token(j);
                if (g.size() != 5) {
                    break;
                }
                const int level = mandatoryLevel(g.substr(0, 2));
                if (level > 0) {
                    auto& l = at(levels, level);
                    if (digits(g, 2, 3)) {
                        l.height = heightOf(level, toInt(g.substr(2, 3)));
                    }
                    temperatureGroup(token(j + 1), l.temperature, l.dewPoint);
                    if (level >= windDownTo) {
                        windGroup(token(j + 2), l.windDirection, l.windSpeed);
                        j += 3;
                    } else {
                        j += 2;   // above the highest level with winds: no wind group
                    }
                } else if (g.compare(0, 2, "88") == 0) {
                    j += g == "88999" ? 1 : 3;   // a tropopause: pressure, temperature, wind
                } else if (g.compare(0, 2, "77") == 0 || g.compare(0, 2, "66") == 0) {
                    break;   // the maximum wind level: not used here
                } else {
                    break;
                }
            }
            haveA = true;
        } else if (t == "XXBB") {
            // YYGGa4 99LaLaLa QcLoLoLoLo MMMULaULo, then nnPPP TTTDD pairs
            size_t j = i + 5;
            while (j + 1 < n && !isKeyword(token(j))) {
                const auto& g = token(j);
                if (g.size() == 5 && digits(g, 0, 5)) {
                    auto& l = at(levels, pressureOf(g.substr(2, 3)));
                    double temperature, dewPoint;
                    temperatureGroup(token(j + 1), temperature, dewPoint);
                    if (UtilityDropsonde::has(temperature)) {
                        l.temperature = temperature;
                        l.dewPoint = dewPoint;
                    }
                }
                j += 2;
            }
        } else if (t == "21212") {
            // nnPPP dddff pairs: the significant levels of wind
            size_t j = i + 1;
            while (j + 1 < n && !isKeyword(token(j))) {
                const auto& g = token(j);
                if (g.size() == 5 && digits(g, 0, 5)) {
                    auto& l = at(levels, pressureOf(g.substr(2, 3)));
                    double direction, speed;
                    windGroup(token(j + 1), direction, speed);
                    if (UtilityDropsonde::has(speed)) {
                        l.windDirection = direction;
                        l.windSpeed = speed;
                    }
                }
                j += 2;
            }
        } else if (t == "31313" && launchHour < 0) {
            const auto& g = token(i + 2);   // 8GGgg
            if (g.size() == 5 && g[0] == '8' && digits(g, 1, 4)) {
                launchHour = toInt(g.substr(1, 2));
                launchMinute = toInt(g.substr(3, 2));
            }
        } else if (t == "61616" && drop.mission.empty()) {
            for (size_t j = i + 1; j < n && !isKeyword(token(j)); j++) {
                drop.mission += (drop.mission.empty() ? "" : " ") + token(j);
            }
        }
    }
    if (!haveA || levels.empty() || fileStamp.size() != 12) {
        return drop;
    }
    // the remarks: lines wrapped at a fixed width, so they are joined without a space (the text breaks mid-word)
    const auto remarksAt = text.find("62626");
    if (remarksAt != string::npos) {
        string body;
        for (size_t k = remarksAt + 5; k < text.size() && text[k] != '='; k++) {
            if (text[k] != '\n' && text[k] != '\r') {
                body.push_back(text[k]);
            }
        }
        const auto first = body.find_first_not_of(' ');
        drop.remarks = first == string::npos ? string{} : body.substr(first);
        static const std::regex rel{R"(REL\s*(\d{4}[NS])\s*(\d{5}[EW]))"};
        static const std::regex spg{R"(SPG\s*(\d{4}[NS])\s*(\d{5}[EW]))"};
        static const std::regex mbl{R"(MBL WND\s*(\d{5}))"};
        std::smatch m;
        if (std::regex_search(drop.remarks, m, rel)) {
            position(m[1], m[2], drop.releaseLat, drop.releaseLon);
        }
        if (std::regex_search(drop.remarks, m, spg)) {
            position(m[1], m[2], drop.splashLat, drop.splashLon);
        }
        if (std::regex_search(drop.remarks, m, mbl)) {
            windGroup(m[1], drop.mblDirection, drop.mblSpeed);
        }
    }
    for (auto it = levels.rbegin(); it != levels.rend(); ++it) {   // the highest pressure (lowest level) first
        drop.levels.push_back(it->second);
    }
    // the time: the day from the report, the month and year from the file's time (the day before that is the previous month); launch time when given
    int year = std::stoi(fileStamp.substr(0, 4));
    int month = std::stoi(fileStamp.substr(4, 2));
    const int fileDay = std::stoi(fileStamp.substr(6, 2));
    if (day == 0) {
        day = fileDay;
    }
    if (day > fileDay) {
        month--;
        if (month == 0) {
            month = 12;
            year--;
        }
    }
    const int hour = launchHour >= 0 ? launchHour : std::max(0, obsHour);
    drop.seconds = daysFromCivil(year, month, day) * 86400L + hour * 3600L + (launchHour >= 0 ? launchMinute * 60L : 0L);
    drop.ok = true;
    return drop;
}

double UtilityDropsonde::minimumPressure(const Drop& drop) {
    if (has(drop.surfacePressure)) {
        return drop.surfacePressure;
    }
    // no surface group: the lowest-pressure level of the surface section is the highest-pressure one that is not below the ground; without
    // the group the first level is the best there is
    return drop.levels.empty() ? missing : drop.levels.front().pressure;
}

double UtilityDropsonde::maxWind(const Drop& drop) {
    double best = missing;
    for (const auto& l : drop.levels) {
        if (has(l.windSpeed) && (!has(best) || l.windSpeed > best)) {
            best = l.windSpeed;
        }
    }
    return best;
}
