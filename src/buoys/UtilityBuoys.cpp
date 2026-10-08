// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "buoys/UtilityBuoys.h"
#include "util/UtilityDate.h"
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <sstream>

namespace {
    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    }

    double value(const string& s) {
        if (s.empty() || s == "MM" || s == "999" || s == "9999") {
            return UtilityBuoys::missing;
        }
        char * end = nullptr;
        const double v = std::strtod(s.c_str(), &end);
        return end != nullptr && *end == '\0' ? v : UtilityBuoys::missing;
    }
}

vector<UtilityBuoys::Obs> UtilityBuoys::parseObservations(const string& text) {
    vector<Obs> out;
    std::istringstream stream{text};
    string line;
    vector<string> columns;
    while (std::getline(stream, line)) {
        std::istringstream words{line};
        vector<string> w;
        string token;
        while (words >> token) {
            w.push_back(token);
        }
        if (w.empty()) {
            continue;
        }
        if (w[0][0] == '#') {
            // the first header line names the columns; the second holds the units
            if (columns.empty()) {
                w[0] = w[0].substr(1);
                if (w[0].empty()) {
                    w.erase(w.begin());
                }
                columns = w;
            }
            continue;
        }
        if (columns.empty() || w.size() != columns.size()) {
            continue;
        }
        Obs o;
        int year = 0, month = 0, day = 0, hour = 0, minute = 0;
        for (size_t i = 0; i < columns.size(); i++) {
            const auto& c = columns[i];
            const auto& v = w[i];
            if (c == "STN") o.id = v;
            else if (c == "LAT") o.lat = value(v);
            else if (c == "LON") o.lon = value(v);
            else if (c == "YYYY" || c == "YY") year = std::atoi(v.c_str());
            else if (c == "MM" && i < 5) month = std::atoi(v.c_str());   // the month column (a data column is also called MM in other files: not here)
            else if (c == "DD") day = std::atoi(v.c_str());
            else if (c == "hh") hour = std::atoi(v.c_str());
            else if (c == "mm") minute = std::atoi(v.c_str());
            else if (c == "WDIR") o.windDirection = value(v);
            else if (c == "WSPD") o.windSpeed = value(v);
            else if (c == "GST") o.gust = value(v);
            else if (c == "WVHT") o.waveHeight = value(v);
            else if (c == "DPD") o.dominantPeriod = value(v);
            else if (c == "APD") o.averagePeriod = value(v);
            else if (c == "MWD") o.waveDirection = value(v);
            else if (c == "PRES") o.pressure = value(v);
            else if (c == "PTDY") o.tendency = value(v);
            else if (c == "ATMP") o.airTemperature = value(v);
            else if (c == "WTMP") o.waterTemperature = value(v);
            else if (c == "DEWP") o.dewPoint = value(v);
            else if (c == "VIS") o.visibility = value(v);
            else if (c == "TIDE") o.tide = value(v);
        }
        if (year < 100) {
            year += 2000;
        }
        if (year < 1990 || month < 1 || month > 12 || day < 1) {
            continue;
        }
        o.seconds = daysFromCivil(year, month, day) * 86400L + hour * 3600L + minute * 60L;
        out.push_back(o);
    }
    return out;
}

std::map<string, UtilityBuoys::Station> UtilityBuoys::parseStations(const string& xml) {
    std::map<string, Station> out;
    static const std::regex station{R"(<station\s+([^>]*?)/?>)"};
    static const std::regex attribute{R"re((\w+)="([^"]*)")re"};
    for (std::sregex_iterator it{xml.begin(), xml.end(), station}, end; it != end; ++it) {
        const string body = (*it)[1];
        std::map<string, string> a;
        for (std::sregex_iterator at{body.begin(), body.end(), attribute}, aend; at != aend; ++at) {
            a[(*at)[1]] = (*at)[2];
        }
        if (!a.contains("id")) {
            continue;
        }
        Station s;
        s.id = a["id"];
        s.lat = value(a["lat"]);
        s.lon = value(a["lon"]);
        s.name = a["name"];
        s.owner = a["owner"];
        s.type = a["type"];
        s.met = a["met"] == "y";
        // the names carry XML entities
        for (auto* field : {&s.name, &s.owner}) {
            for (const auto& [from, to] : {std::pair<const char *, const char *>{"&amp;", "&"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&lt;", "<"}, {"&gt;", ">"}}) {
                size_t pos = 0;
                const string f = from;
                while ((pos = field->find(f, pos)) != string::npos) {
                    field->replace(pos, f.size(), to);
                    pos += string{to}.size();
                }
            }
        }
        out[s.id] = s;
    }
    return out;
}

string UtilityBuoys::timeText(long seconds) {
    return UtilityDate::isoMinute(seconds);   // "2026-10-07 01:22Z"
}
