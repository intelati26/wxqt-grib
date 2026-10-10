// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "obs/UtilityMetarCache.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <numbers>
#include <sstream>

namespace {
    double number(const string& s) {
        if (s.empty()) {
            return SurfaceStation::missing;
        }
        char * end = nullptr;
        const double v = std::strtod(s.c_str(), &end);
        return end == s.c_str() ? SurfaceStation::missing : v;
    }

    double mercatorOf(double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    }

    // "10+" is ten or more
    double visibility(const string& s) {
        if (s.empty()) {
            return SurfaceStation::missing;
        }
        return number(s.back() == '+' ? s.substr(0, s.size() - 1) : s);
    }
}

vector<string> UtilityMetarCache::splitCsv(const string& line) {
    vector<string> fields;
    string field;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); i++) {
        const char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field.push_back('"');
                    i++;
                } else {
                    quoted = false;
                }
            } else {
                field.push_back(c);
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(field);
            field.clear();
        } else if (c != '\r') {
            field.push_back(c);
        }
    }
    fields.push_back(field);
    return fields;
}

long UtilityMetarCache::parseTime(const string& iso) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (std::sscanf(iso.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) < 5 || y < 1970) {
        return 0;
    }
    // days since 1970-01-01 of a civil date (Howard Hinnant's algorithm), so no platform-specific timegm is needed
    const long long yy = mo <= 2 ? y - 1 : y;
    const long long era = (yy >= 0 ? yy : yy - 399) / 400;
    const long long yoe = yy - era * 400;
    const long long doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = era * 146097 + doe - 719468;
    return static_cast<long>(days * 86400 + h * 3600 + mi * 60 + s);
}

// the columns, by position (the four sky cover / cloud base pairs repeat their names): raw_text 0, station_id 1, observation_time 2, latitude 3,
// longitude 4, temp_c 5, dewpoint_c 6, wind_dir_degrees 7, wind_speed_kt 8, wind_gust_kt 9, visibility_statute_mi 10, altim_in_hg 11,
// sea_level_pressure_mb 12, ... wx_string 21, sky_cover / cloud_base_ft_agl 22-29, flight_category 30, ... metar_type 42, elevation_m 43
vector<SurfaceStation> UtilityMetarCache::parse(const string& csv) {
    vector<SurfaceStation> stations;
    std::istringstream in{csv};
    string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) {
            first = false;
            if (line.rfind("raw_text", 0) == 0) {
                continue;
            }
        }
        const auto f = splitCsv(line);
        if (f.size() < 31) {
            continue;
        }
        SurfaceStation s;
        s.lat = number(f[3]);
        s.lon = number(f[4]);
        if (!SurfaceStation::has(s.lat) || !SurfaceStation::has(s.lon) || std::abs(s.lat) > 89.0) {
            continue;
        }
        s.raw = f[0];
        s.id = f[1];
        s.network = "METAR";
        s.airport = true;
        s.seconds = parseTime(f[2]);
        s.mercator = mercatorOf(s.lat);
        s.temperature = number(f[5]);
        s.dewPoint = number(f[6]);
        s.windDirection = number(f[7]);
        s.windSpeed = number(f[8]);
        s.windGust = number(f[9]);
        s.visibility = visibility(f[10]);
        s.altimeter = number(f[11]);
        s.seaLevel = number(f[12]);
        s.weather = f[21];
        for (size_t i = 22; i + 1 < 30; i += 2) {
            if (!f[i].empty()) {
                s.sky += (s.sky.empty() ? "" : ", ") + f[i] + (f[i + 1].empty() ? "" : " " + f[i + 1] + " ft");
            }
        }
        s.flight = f[30];
        if (f.size() > 43) {
            s.elevation = number(f[43]);
        }
        if (SurfaceStation::has(s.temperature) && SurfaceStation::has(s.dewPoint)) {   // Magnus formula
            const auto saturation = [] (double t) { return std::exp(17.625 * t / (243.04 + t)); };
            s.humidity = std::min(100.0, 100.0 * saturation(s.dewPoint) / saturation(s.temperature));
        }
        stations.push_back(std::move(s));
    }
    return stations;
}

namespace {
    // the string value of "key":"..." inside one JSON object's text, with the simple escapes undone; empty for null or absent
    string field(const string& object, const char * key) {
        const string needle = string{"\""} + key + "\":";
        const auto at = object.find(needle);
        if (at == string::npos) {
            return {};
        }
        size_t pos = at + needle.size();
        if (pos >= object.size() || object[pos] != '"') {
            return {};
        }
        string value;
        for (pos++; pos < object.size() && object[pos] != '"'; pos++) {
            if (object[pos] == '\\' && pos + 1 < object.size()) {
                pos++;
                if (object[pos] == 'u' && pos + 4 < object.size()) {
                    const unsigned code = static_cast<unsigned>(std::strtoul(object.substr(pos + 1, 4).c_str(), nullptr, 16));
                    if (code < 0x80) {
                        value.push_back(static_cast<char>(code));
                    } else if (code < 0x800) {
                        value.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        value.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    } else {
                        value.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        value.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        value.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                    pos += 4;
                } else {
                    value.push_back(object[pos] == 'n' ? ' ' : object[pos]);
                }
            } else {
                value.push_back(object[pos]);
            }
        }
        return value;
    }
}

std::map<string, UtilityMetarCache::Name> UtilityMetarCache::parseNames(const string& json) {
    std::map<string, Name> names;
    size_t pos = 0;
    while ((pos = json.find('{', pos)) != string::npos) {
        const auto end = json.find('}', pos);
        if (end == string::npos) {
            break;
        }
        const auto object = json.substr(pos, end - pos + 1);
        pos = end + 1;
        const Name name{field(object, "site"), field(object, "state")};
        for (const char * key : {"id", "icaoId"}) {
            const auto id = field(object, key);
            if (!id.empty()) {
                names.emplace(id, name);
            }
        }
    }
    return names;
}
