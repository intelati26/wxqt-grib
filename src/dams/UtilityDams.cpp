// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "dams/UtilityDams.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numbers>
#include <regex>
#include <sstream>

namespace {
    vector<string> tabs(const string& line) {
        vector<string> parts;
        size_t start = 0;
        while (true) {
            const auto tab = line.find('\t', start);
            parts.push_back(line.substr(start, tab == string::npos ? string::npos : tab - start));
            if (tab == string::npos) {
                break;
            }
            start = tab + 1;
        }
        return parts;
    }

    string series(const string& s) {
        return s == "-" ? string{} : s;
    }
}

vector<UtilityDams::Project> UtilityDams::parseRegistry(const string& text) {
    vector<Project> out;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        const auto p = tabs(line);
        if (p.size() < 13) {
            continue;
        }
        Project project;
        project.office = p[0];
        project.id = p[1];
        project.name = p[2];
        project.lat = std::atof(p[3].c_str());
        project.lon = std::atof(p[4].c_str());
        project.city = p[5];
        project.state = p[6];
        project.pool = series(p[7]);
        project.tailwater = series(p[8]);
        project.outflow = series(p[9]);
        project.power = series(p[10]);
        project.inflow = series(p[11]);
        size_t start = 0;
        while (start <= p[12].size() && !p[12].empty()) {
            const auto semicolon = p[12].find(';', start);
            project.generation.push_back(p[12].substr(start, semicolon == string::npos ? string::npos : semicolon - start));
            if (semicolon == string::npos) {
                break;
            }
            start = semicolon + 1;
        }
        project.mercator = 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + project.lat * std::numbers::pi / 360.0));
        out.push_back(std::move(project));
    }
    return out;
}

UtilityDams::Series UtilityDams::parseTimeSeries(const string& json) {
    Series result;
    static const std::regex units{R"re("units"\s*:\s*"([^"]*)")re"};
    std::smatch m;
    if (std::regex_search(json, m, units)) {
        result.units = m[1];
    }
    const auto at = json.find("\"values\"");
    if (at == string::npos) {
        return result;
    }
    // [1791345600000, 0.0, 0]  (a missing value is null)
    static const std::regex row{R"(\[\s*(\d+)\s*,\s*(null|-?[\d.]+(?:[eE][-+]?\d+)?)\s*,\s*-?\d+\s*\])"};
    const string tail = json.substr(at);
    for (std::sregex_iterator it{tail.begin(), tail.end(), row}, end; it != end; ++it) {
        if ((*it)[2] == "null") {
            continue;
        }
        Point p;
        p.seconds = static_cast<long>(std::stoll((*it)[1]) / 1000);
        p.value = std::strtod((*it)[2].str().c_str(), nullptr);
        result.points.push_back(p);
    }
    return result;
}

UtilityDams::Series UtilityDams::sum(const vector<Series>& all) {
    Series total;
    std::map<long, double> bySecond;
    for (const auto& s : all) {
        if (total.units.empty()) {
            total.units = s.units;
        }
        for (const auto& p : s.points) {
            bySecond[p.seconds] += p.value;
        }
    }
    for (const auto& [seconds, value] : bySecond) {
        total.points.push_back({seconds, value});
    }
    return total;
}

double UtilityDams::kilometers(double lat1, double lon1, double lat2, double lon2) {
    const double rad = std::numbers::pi / 180.0;
    const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
    return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
}

const UtilityDams::Project * UtilityDams::nearest(const vector<Project>& projects, double lat, double lon, double maxKm, double * distanceKm) {
    const Project * best = nullptr;
    double bestDistance = maxKm;
    for (const auto& p : projects) {
        const double d = kilometers(lat, lon, p.lat, p.lon);
        if (d <= bestDistance) {
            bestDistance = d;
            best = &p;
        }
    }
    if (best != nullptr && distanceKm != nullptr) {
        *distanceKm = bestDistance;
    }
    return best;
}
