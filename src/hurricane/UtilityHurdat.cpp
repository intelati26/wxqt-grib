// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityHurdat.h"
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
