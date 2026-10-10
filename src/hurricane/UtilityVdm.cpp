// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityVdm.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include <regex>
#include <sstream>

namespace {
    const double none = UtilityVdm::missing;

    string trim(const string& s) {
        const auto a = s.find_first_not_of(" \t\r");
        if (a == string::npos) {
            return "";
        }
        return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    }

    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    double number(const std::smatch& m, size_t i) {
        return std::strtod(m[i].str().c_str(), nullptr);
    }

    // "329 deg 33 nm 00:03:45Z"
    void bearingRange(const string& value, UtilityVdm::Wind& wind) {
        static const std::regex re{R"((\d+)\s*deg\s+(\d+(?:\.\d+)?)\s*nm\s+(\d\d:\d\d:\d\dZ))"};
        std::smatch m;
        if (std::regex_search(value, m, re)) {
            wind.bearing = number(m, 1);
            wind.rangeNm = number(m, 2);
            wind.time = m[3];
        }
    }

    // "062 deg 29 kt" (a flight-level maximum: its direction and speed) or "90 kt" (an estimated surface maximum)
    void windValue(const string& value, UtilityVdm::Wind& wind) {
        static const std::regex both{R"((\d+)\s*deg\s+(\d+)\s*kt)"};
        static const std::regex only{R"((\d+)\s*kt)"};
        std::smatch m;
        if (std::regex_search(value, m, both)) {
            wind.direction = number(m, 1);
            wind.kt = number(m, 2);
        } else if (std::regex_search(value, m, only)) {
            wind.kt = number(m, 1);
        }
    }

    // "15 C / 3063 m": the temperature, and the pressure altitude
    double celsius(const string& value, bool second = false) {
        static const std::regex re{R"((-?\d+(?:\.\d+)?)\s*C\s*/\s*(\S+))"};
        std::smatch m;
        if (!std::regex_search(value, m, re)) {
            return none;
        }
        if (!second) {
            return number(m, 1);
        }
        return m[2].str() == "NA" ? none : std::strtod(m[2].str().c_str(), nullptr);
    }
}

double UtilityVdm::Vdm::maxFlightWind() const {
    return std::max(has(inboundFlight.kt) ? inboundFlight.kt : none, has(outboundFlight.kt) ? outboundFlight.kt : none);
}

bool UtilityVdm::parse(const string& text, const string& fileStamp, Vdm& out) {
    out = Vdm{};
    std::istringstream stream{text};
    string line;
    std::map<char, string> items;
    vector<string> rest;
    bool afterItems = false;
    static const std::regex header{R"(VORTEX DATA MESSAGE\s+([A-Z]{2}\d{6}))"};
    while (std::getline(stream, line)) {
        line = trim(line);
        std::smatch m;
        if (std::regex_search(line, m, header)) {
            out.stormId = m[1];
            continue;
        }
        if (line.size() >= 2 && line[1] == '.' && line[0] >= 'A' && line[0] <= 'U' && !afterItems) {
            items[line[0]] = trim(line.substr(2));
            if (line[0] == 'U') {
                afterItems = true;
            }
        } else if (afterItems && !line.empty()) {
            rest.push_back(line);
        }
    }
    if (out.stormId.empty() || !items.contains('A')) {
        return false;
    }
    for (const auto& r : rest) {
        out.remarks += (out.remarks.empty() ? "" : "; ") + r;
    }
    out.test = text.find("TEST") != string::npos && text.find("COMM CHECK") != string::npos;
    // A: 22/00:11:26Z, with the month and year from the file's time (the day before that is the previous month)
    static const std::regex timeRe{R"((\d\d)/(\d\d):(\d\d):(\d\d)Z)"};
    std::smatch m;
    if (!std::regex_search(items['A'], m, timeRe) || fileStamp.size() != 12) {
        return false;
    }
    out.fixTime = m[0];
    int year = std::stoi(fileStamp.substr(0, 4));
    int month = std::stoi(fileStamp.substr(4, 2));
    const int fileDay = std::stoi(fileStamp.substr(6, 2));
    const int day = std::stoi(m[1]);
    if (day > fileDay) {
        month--;
        if (month == 0) {
            month = 12;
            year--;
        }
    }
    out.seconds = daysFromCivil(year, month, day) * 86400L + std::stoi(m[2]) * 3600L + std::stoi(m[3]) * 60L + std::stoi(m[4]);
    static const std::regex position{R"((\d+(?:\.\d+)?)\s*deg\s*([NS])\s+(\d+(?:\.\d+)?)\s*deg\s*([EW]))"};
    if (std::regex_search(items['B'], m, position)) {
        out.lat = number(m, 1) * (m[2] == "S" ? -1.0 : 1.0);
        out.lon = number(m, 3) * (m[4] == "W" ? -1.0 : 1.0);
    }
    static const std::regex height{R"((\d+)\s*MB\s+(\d+)\s*m)", std::regex::icase};
    if (std::regex_search(items['C'], m, height)) {
        out.levelMb = std::stoi(m[1]);
        out.heightM = number(m, 2);
    }
    static const std::regex pressure{R"((\d+)\s*mb)", std::regex::icase};
    if (std::regex_search(items['D'], m, pressure)) {
        out.pressure = number(m, 1);
        out.extrapolated = items['D'].find("EXTRAP") != string::npos;
    }
    static const std::regex centerWind{R"((\d+)\s*deg\s+(\d+)\s*kt)"};
    if (std::regex_search(items['E'], m, centerWind)) {
        out.centerWindDir = number(m, 1);
        out.centerWindKt = number(m, 2);
    }
    const auto text2 = [&] (char c) { return items[c] == "NA" ? string{} : items[c]; };
    out.eyeCharacter = text2('F');
    out.eyeShape = text2('G');
    windValue(items['H'], out.inboundSurface);
    bearingRange(items['I'], out.inboundSurface);
    windValue(items['J'], out.inboundFlight);
    bearingRange(items['K'], out.inboundFlight);
    windValue(items['L'], out.outboundSurface);
    bearingRange(items['M'], out.outboundSurface);
    windValue(items['N'], out.outboundFlight);
    bearingRange(items['O'], out.outboundFlight);
    out.tempOutsideC = celsius(items['P']);
    out.tempInsideC = celsius(items['Q']);
    out.dewPointInsideC = celsius(items['R']);
    out.seaSurfaceC = celsius(items['R'], true) ;
    out.fixedBy = text2('S');
    out.accuracy = text2('T');
    out.aircraft = text2('U');
    return true;
}
