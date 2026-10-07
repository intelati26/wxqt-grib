// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityAtcf.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>

namespace {
    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r\n");
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
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

    // "219N" -> 21.9, "957W" -> -95.7 (tenths of a degree with a hemisphere letter)
    bool coordinate(const string& s, double& value) {
        if (s.size() < 2) {
            return false;
        }
        const char hemisphere = s.back();
        if (hemisphere != 'N' && hemisphere != 'S' && hemisphere != 'E' && hemisphere != 'W') {
            return false;
        }
        char * end = nullptr;
        const auto tenths = std::strtol(s.substr(0, s.size() - 1).c_str(), &end, 10);
        if (end == nullptr || *end != '\0') {
            return false;
        }
        value = tenths / 10.0;
        if (hemisphere == 'S' || hemisphere == 'W') {
            value = -value;
        }
        return true;
    }

    int toInt(const string& s, int missing) {
        if (s.empty()) {
            return missing;
        }
        char * end = nullptr;
        const auto value = std::strtol(s.c_str(), &end, 10);
        return end != nullptr && *end == '\0' ? static_cast<int>(value) : missing;
    }

    // days since 1970-01-01 for a civil date
    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }
}

vector<UtilityAtcf::Fix> UtilityAtcf::parseRows(const string& text, const string& onlyTech) {
    vector<Fix> rows;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        const auto parts = splitComma(line);
        if (parts.size() < 11 || parts[2].size() != 10) {
            continue;
        }
        if (!onlyTech.empty() && parts[4] != onlyTech) {
            continue;
        }
        Fix fix;
        if (!coordinate(parts[6], fix.lat) || !coordinate(parts[7], fix.lon)) {
            continue;
        }
        fix.time = parts[2];
        fix.tau = toInt(parts[5], 0);
        fix.wind = toInt(parts[8], -1);
        fix.pressure = toInt(parts[9], -1);
        if (fix.wind <= 0) {
            fix.wind = -1;
        }
        if (fix.pressure <= 0) {
            fix.pressure = -1;
        }
        fix.status = parts[10];
        if (parts.size() > 27) {
            fix.name = parts[27];
        }
        rows.push_back(fix);
    }
    return rows;
}

vector<UtilityAtcf::Fix> UtilityAtcf::bestTrack(const string& btkText) {
    vector<Fix> best;
    std::set<string> seen;
    for (auto& fix : parseRows(btkText, "BEST")) {
        if (seen.insert(fix.time).second) {
            best.push_back(fix);
        }
    }
    return best;
}

vector<UtilityAtcf::Track> UtilityAtcf::latestTracks(const string& text) {
    // technique -> cycle -> forecast hour -> fix
    std::map<string, std::map<string, std::map<int, Fix>>> byTech;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        const auto parts = splitComma(line);
        if (parts.size() < 11 || parts[2].size() != 10) {
            continue;
        }
        Fix fix;
        if (!coordinate(parts[6], fix.lat) || !coordinate(parts[7], fix.lon)) {
            continue;
        }
        if (std::abs(fix.lat) > 90.0 || std::abs(fix.lon) > 180.0 || (fix.lat == 0.0 && fix.lon == 0.0)) {
            continue;   // 0N 0W is how a technique says "no position" (IVCN does)
        }
        fix.time = parts[2];
        fix.tau = toInt(parts[5], -1);
        if (fix.tau < 0) {
            continue;
        }
        fix.wind = toInt(parts[8], -1);
        fix.pressure = toInt(parts[9], -1);
        if (fix.wind <= 0) {
            fix.wind = -1;
        }
        if (fix.pressure <= 0) {
            fix.pressure = -1;
        }
        fix.status = parts[10];
        byTech[parts[4]][parts[2]].emplace(fix.tau, fix);   // the first row of an hour (later rows only add wind radii)
    }
    vector<Track> tracks;
    for (const auto& [tech, cycles] : byTech) {
        const auto& newest = *cycles.rbegin();   // the largest yyyymmddhh
        Track track;
        track.tech = tech;
        track.cycle = newest.first;
        for (const auto& [tau, fix] : newest.second) {
            track.fixes.push_back(fix);
        }
        if (track.fixes.size() >= 2) {
            tracks.push_back(track);
        }
    }
    return tracks;
}

std::map<string, string> UtilityAtcf::parseTechList(const string& text) {
    std::map<string, string> names;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        std::istringstream words{line};
        string num, tech, a, b, c, d, e, f;
        if (!(words >> num >> tech >> a >> b >> c >> d >> e >> f) || num == "NUM") {
            continue;
        }
        string rest;
        std::getline(words, rest);
        rest = trim(rest);
        if (!rest.empty()) {
            names.emplace(tech, rest);
        }
    }
    return names;
}

UtilityAtcf::Group UtilityAtcf::groupOf(const string& tech) {
    static const std::set<string> official{"OFCL", "OFCI", "OFC2", "OFCP", "OFPI", "OFP2", "OFCO"};
    static const std::set<string> consensus{"TVCN", "TVCX", "TVCA", "TVCE", "TVCC", "TVDG", "IVCN", "IVCX", "ICON", "CCON", "HCCA", "HCCI",
        "GFEX", "TCOA", "TCON", "TCCN", "NNIC", "AEMI", "AEMN", "EEMN", "CEMN", "UEMN", "FSSE"};
    static const std::set<string> global{"AVNO", "AVNI", "AVNX", "AVXI", "GFSO", "GFSI", "EMX", "EMXI", "EMX2", "ECMF", "EMDT", "CMC", "CMCI", "NGX",
        "NGXI", "NGX2", "NVGM", "NVGI", "UKM", "UKMI", "UKX", "UKXI", "UKX2", "JGSM", "JGSI", "NAM", "NAMI", "AIFS", "AIFI", "GDMI", "GDMN", "FNV3", "FNVI",
        "AIDA", "GRAP", "AVNP", "PANG", "GOES"};
    static const std::set<string> hurricane{"HWRF", "HWFI", "HMON", "HMNI", "HFSA", "HFAI", "HFSB", "HFBI", "HAFA", "HAFB", "CTCX", "CTCI", "COTC", "COTI", "HWRP", "HWFP", "GFDL", "GHMI"};
    static const std::set<string> simple{"TABS", "TABM", "TABD", "TABE", "BAMS", "BAMM", "BAMD", "BAMA", "BAMB", "LBAR", "CLIP", "CLP5", "XTRP", "A98E", "A97E", "TCLP", "NHC5", "OCD5", "BCD5", "SHF5", "SHIP", "DSHP", "LGEM", "SHFR", "DRCL", "DRCC", "AFWA"};
    if (official.contains(tech)) {
        return Group::Official;
    }
    if (consensus.contains(tech)) {
        return Group::Consensus;
    }
    if (global.contains(tech)) {
        return Group::Global;
    }
    if (hurricane.contains(tech)) {
        return Group::Hurricane;
    }
    if (simple.contains(tech)) {
        return Group::Simple;
    }
    // ensemble members: AP01 (GFS ensemble), AC00 (its control), EE01 .. (ECMWF ensemble), CE01 ..., UE01 ...
    if (tech.size() == 4 && std::isdigit(static_cast<unsigned char>(tech[2])) && std::isdigit(static_cast<unsigned char>(tech[3]))) {
        return Group::Ensemble;
    }
    return Group::Other;
}

string UtilityAtcf::groupName(Group group) {
    switch (group) {
        case Group::Official: return "NHC official";
        case Group::Consensus: return "Consensus and means";
        case Group::Global: return "Global models";
        case Group::Hurricane: return "Hurricane models";
        case Group::Ensemble: return "Ensemble members";
        case Group::Simple: return "Simple and statistical";
        default: return "Other guidance";
    }
}

int UtilityAtcf::categoryOf(int wind) {
    if (wind < 34) return 0;
    if (wind < 64) return 1;
    if (wind < 83) return 2;
    if (wind < 96) return 3;
    if (wind < 113) return 4;
    if (wind < 137) return 5;
    return 6;
}

string UtilityAtcf::categoryName(int category) {
    static const char * names[] = {"Tropical depression", "Tropical storm", "Category 1", "Category 2", "Category 3", "Category 4", "Category 5"};
    return names[std::clamp(category, 0, 6)];
}

double UtilityAtcf::hoursBetween(const string& a, const string& b) {
    const auto hours = [] (const string& t) -> double {
        if (t.size() != 10) {
            return 0.0;
        }
        return daysFromCivil(std::stoi(t.substr(0, 4)), std::stoi(t.substr(4, 2)), std::stoi(t.substr(6, 2))) * 24.0 + std::stoi(t.substr(8, 2));
    };
    return hours(b) - hours(a);
}

string UtilityAtcf::formatTime(const string& t) {
    static const char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (t.size() != 10) {
        return t;
    }
    const int month = std::stoi(t.substr(4, 2));
    return string{months[std::clamp(month, 1, 12) - 1]} + " " + t.substr(6, 2) + " " + t.substr(8, 2) + "Z";
}
