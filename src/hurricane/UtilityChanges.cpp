// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityChanges.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <numbers>
#include <sstream>
#include "hurricane/UtilityAtcf.h"

namespace {
    string number(double value, int digits) {
        char buffer[32];
        std::snprintf(buffer, sizeof buffer, "%.*f", digits, value);
        return buffer;
    }
}

double UtilityChanges::distanceKm(double lat1, double lon1, double lat2, double lon2) {
    const double rad = std::numbers::pi / 180.0;
    const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
    return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
}

double UtilityChanges::bearing(double lat1, double lon1, double lat2, double lon2) {
    const double rad = std::numbers::pi / 180.0;
    const double y = std::sin((lon2 - lon1) * rad) * std::cos(lat2 * rad);
    const double x = std::cos(lat1 * rad) * std::sin(lat2 * rad) - std::sin(lat1 * rad) * std::cos(lat2 * rad) * std::cos((lon2 - lon1) * rad);
    double degrees = std::atan2(y, x) / rad;
    return degrees < 0 ? degrees + 360.0 : degrees;
}

string UtilityChanges::serialize(const Snapshot& s) {
    std::ostringstream out;
    out << "adv=" << s.advisory << ";cls=" << s.classification << ";w=" << s.wind << ";p=" << s.pressure << ";lat=" << number(s.lat, 2) << ";lon=" << number(s.lon, 2)
        << ";dir=" << s.moveDir << ";spd=" << s.moveSpeed << ";pk=" << s.forecastPeak << ";pkh=" << s.forecastPeakHour << ";ri=" << number(s.ri30, 1);
    return out.str();
}

UtilityChanges::Snapshot UtilityChanges::parse(const string& text) {
    std::map<string, string> values;
    std::istringstream stream{text};
    string item;
    while (std::getline(stream, item, ';')) {
        const auto eq = item.find('=');
        if (eq != string::npos) {
            values[item.substr(0, eq)] = item.substr(eq + 1);
        }
    }
    const auto num = [&] (const char * key, double fallback) {
        const auto found = values.find(key);
        return found == values.end() || found->second.empty() ? fallback : std::strtod(found->second.c_str(), nullptr);
    };
    Snapshot s;
    s.advisory = values["adv"];
    s.classification = values["cls"];
    s.wind = static_cast<int>(num("w", -1));
    s.pressure = static_cast<int>(num("p", -1));
    s.lat = num("lat", 0.0);
    s.lon = num("lon", 0.0);
    s.moveDir = static_cast<int>(num("dir", -1));
    s.moveSpeed = static_cast<int>(num("spd", -1));
    s.forecastPeak = static_cast<int>(num("pk", -1));
    s.forecastPeakHour = static_cast<int>(num("pkh", -1));
    s.ri30 = num("ri", -1.0);
    return s;
}

vector<string> UtilityChanges::describe(const Snapshot& a, const Snapshot& b) {
    vector<string> lines;
    if (!a.valid() || !b.valid() || a.advisory == b.advisory) {
        return lines;
    }
    if (!a.classification.empty() && !b.classification.empty() && a.classification != b.classification) {
        lines.push_back("Classification " + a.classification + " to " + b.classification);
    }
    if (a.wind >= 0 && b.wind >= 0 && a.wind != b.wind) {
        lines.push_back("Winds " + UtilityAtcf::windLabel(a.wind) + " to " + UtilityAtcf::windLabel(b.wind) + " (" + (b.wind > a.wind ? "+" : "") + std::to_string(b.wind - a.wind) + " kt)");
    } else if (a.wind >= 0 && a.wind == b.wind) {
        lines.push_back("Winds unchanged at " + UtilityAtcf::windLabel(b.wind));
    }
    if (a.pressure > 0 && b.pressure > 0 && a.pressure != b.pressure) {
        lines.push_back("Pressure " + std::to_string(a.pressure) + " to " + std::to_string(b.pressure) + " mb (" + (b.pressure > a.pressure ? "+" : "") + std::to_string(b.pressure - a.pressure) + ")");
    }
    const double moved = distanceKm(a.lat, a.lon, b.lat, b.lon);
    if (moved >= 5.0 && (a.lat != 0.0 || a.lon != 0.0)) {
        lines.push_back("Centre moved " + number(moved, 0) + " km toward " + number(bearing(a.lat, a.lon, b.lat, b.lon), 0) + " degrees");
    }
    if (b.moveSpeed >= 0 && (a.moveDir != b.moveDir || a.moveSpeed != b.moveSpeed) && a.moveSpeed >= 0) {
        lines.push_back("Motion " + std::to_string(a.moveDir) + " deg at " + std::to_string(a.moveSpeed) + " kt to " + std::to_string(b.moveDir) + " deg at " + std::to_string(b.moveSpeed) + " kt");
    }
    if (a.forecastPeak >= 0 && b.forecastPeak >= 0 && (a.forecastPeak != b.forecastPeak || a.forecastPeakHour != b.forecastPeakHour)) {
        lines.push_back("NHC forecast peak " + UtilityAtcf::windLabel(a.forecastPeak) + " at " + std::to_string(a.forecastPeakHour) + " h to " + UtilityAtcf::windLabel(b.forecastPeak) + " at " +
                        std::to_string(b.forecastPeakHour) + " h");
    }
    if (a.ri30 >= 0.0 && b.ri30 >= 0.0 && std::abs(a.ri30 - b.ri30) >= 1.0) {
        lines.push_back("SHIPS-RII chance of a 30 kt rise in 24 h " + number(a.ri30, 0) + " % to " + number(b.ri30, 0) + " %");
    }
    return lines;
}
