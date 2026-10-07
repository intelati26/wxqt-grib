// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilitySeason.h"
#include <algorithm>
#include <cstdlib>
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

    void note(UtilitySeason::Storm& storm, const string& time, const string& status, int wind, int pressure, int hour) {
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
        storm.ace += UtilitySeason::recordAce(hour, status, wind);
    }
}

double UtilitySeason::recordAce(int hourUtc, const string& status, int windKt) {
    if (hourUtc % 6 != 0 || windKt < 34 || !(status == "TS" || status == "SS" || status == "HU")) {
        return 0.0;
    }
    return static_cast<double>(windKt) * windKt / 10000.0;
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
        note(storms.back(), parts[0] + parts[1].substr(0, 2), parts[3], wind > 0 ? wind : 0, pressure, hourForAce);
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
        note(storm, f.time, f.status, f.wind > 0 ? f.wind : 0, f.pressure, hour);
    }
    if (storm.name.empty()) {
        storm.name = "UNNAMED";
    }
    return storm;
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
    }
    vector<Season> out;
    for (const auto& [year, season] : byYear) {
        out.push_back(season);
    }
    return out;
}

string UtilitySeason::csv(const vector<Storm>& storms) {
    std::ostringstream out;
    for (const auto& s : storms) {
        out << s.id << ',' << s.name << ',' << s.year << ',' << s.first << ',' << s.last << ',' << s.peakWind << ',' << s.minPressure << ',' << s.ace << ','
            << (s.stormStrength ? 1 : 0) << '\n';
    }
    return out.str();
}

vector<UtilitySeason::Storm> UtilitySeason::fromCsv(const string& text) {
    vector<Storm> storms;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        const auto p = splitComma(line);
        if (p.size() != 9) {
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
        storms.push_back(s);
    }
    return storms;
}

string UtilitySeason::categoryName(int wind) {
    return UtilityAtcf::categoryName(UtilityAtcf::categoryOf(wind));
}

string UtilitySeason::newestHurdatFile(const string& listing) {
    // hurdat2-1851-2025-092326.txt: the date is month day year, with a two or four digit year (02272026 is 27 February 2026)
    static const std::regex re{R"re(hurdat2-1851-(\d{4})-(\d{2})(\d{2})(\d{2}|\d{4})\.txt)re"};
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
