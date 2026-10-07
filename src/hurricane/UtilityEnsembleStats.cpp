// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityEnsembleStats.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>

double UtilityEnsembleStats::percentile(vector<double> values, double fraction) {
    if (values.empty()) {
        return missing;
    }
    std::sort(values.begin(), values.end());
    const double rank = std::clamp(fraction, 0.0, 1.0) * static_cast<double>(values.size() - 1);
    const auto low = static_cast<size_t>(std::floor(rank));
    const auto high = static_cast<size_t>(std::ceil(rank));
    return values[low] + (values[high] - values[low]) * (rank - static_cast<double>(low));
}

double UtilityEnsembleStats::kilometers(double lat1, double lon1, double lat2, double lon2) {
    const double rad = std::numbers::pi / 180.0;
    const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) +
        std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
    return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
}

vector<UtilityEnsembleStats::Hour> UtilityEnsembleStats::compute(const UtilityEcmwfTracks::Storm& storm, int step) {
    vector<const UtilityEcmwfTracks::Member *> members;
    int longest = 0;
    for (const auto& member : storm.members) {
        if (member.type >= 2) {   // the perturbed members; the unperturbed runs are drawn on their own
            members.push_back(&member);
            for (const auto& s : member.steps) {
                longest = std::max(longest, s.hour);
            }
        }
    }
    vector<Hour> hours;
    if (members.empty()) {
        return hours;
    }
    for (int hour = 0; hour <= longest; hour += step) {
        Hour h;
        h.hour = hour;
        h.total = static_cast<int>(members.size());
        vector<double> winds, pressures;
        vector<std::pair<double, double>> positions;
        int ts = 0, hurricane = 0, major = 0;
        for (const auto * member : members) {
            const UtilityEcmwfTracks::Step * found = nullptr;
            for (const auto& s : member->steps) {
                if (s.hour == hour) {
                    found = &s;
                    break;
                }
            }
            if (found == nullptr || !UtilityEcmwfTracks::has(found->lat) || !UtilityEcmwfTracks::has(found->lon)) {
                continue;   // the member has no cyclone at this hour
            }
            h.alive++;
            positions.emplace_back(found->lat, found->lon);
            if (UtilityEcmwfTracks::has(found->wind)) {
                winds.push_back(found->wind);
                ts += found->wind >= 34.0 ? 1 : 0;
                hurricane += found->wind >= 64.0 ? 1 : 0;
                major += found->wind >= 96.0 ? 1 : 0;
            }
            if (UtilityEcmwfTracks::has(found->pressure)) {
                pressures.push_back(found->pressure);
            }
        }
        const double total = static_cast<double>(h.total);
        h.probAlive = h.alive / total;
        h.probTs = ts / total;
        h.probHurricane = hurricane / total;
        h.probMajor = major / total;
        if (!winds.empty()) {
            h.windMin = percentile(winds, 0.0);
            h.wind10 = percentile(winds, 0.1);
            h.wind25 = percentile(winds, 0.25);
            h.windMedian = percentile(winds, 0.5);
            h.wind75 = percentile(winds, 0.75);
            h.wind90 = percentile(winds, 0.9);
            h.windMax = percentile(winds, 1.0);
        }
        if (!pressures.empty()) {
            h.pressMin = percentile(pressures, 0.0);
            h.press10 = percentile(pressures, 0.1);
            h.press25 = percentile(pressures, 0.25);
            h.pressMedian = percentile(pressures, 0.5);
            h.press75 = percentile(pressures, 0.75);
            h.press90 = percentile(pressures, 0.9);
            h.pressMax = percentile(pressures, 1.0);
        }
        if (!positions.empty()) {
            // the mean position as the mean of the unit vectors, so longitudes either side of 180 degrees do not cancel
            double x = 0.0, y = 0.0, z = 0.0;
            const double rad = std::numbers::pi / 180.0;
            for (const auto& [lat, lon] : positions) {
                x += std::cos(lat * rad) * std::cos(lon * rad);
                y += std::cos(lat * rad) * std::sin(lon * rad);
                z += std::sin(lat * rad);
            }
            h.centerLat = std::atan2(z, std::hypot(x, y)) / rad;
            h.centerLon = std::atan2(y, x) / rad;
            vector<double> distances;
            for (const auto& [lat, lon] : positions) {
                distances.push_back(kilometers(h.centerLat, h.centerLon, lat, lon));
            }
            h.radius50 = percentile(distances, 0.5);
            h.radius90 = percentile(distances, 0.9);
        }
        hours.push_back(h);
    }
    return hours;
}

bool UtilityEnsembleStats::fromGefs(const vector<UtilityAtcf::Track>& guidance, UtilityEcmwfTracks::Storm& storm) {
    storm = UtilityEcmwfTracks::Storm{};
    for (const auto& track : guidance) {
        const bool member = track.tech.size() == 4 && track.tech.compare(0, 2, "AP") == 0 && std::isdigit(static_cast<unsigned char>(track.tech[2])) &&
            std::isdigit(static_cast<unsigned char>(track.tech[3]));
        const bool control = track.tech == "AC00";
        if (!member && !control) {
            continue;
        }
        UtilityEcmwfTracks::Member m;
        m.number = control ? 0 : std::stoi(track.tech.substr(2));
        m.type = control ? 1 : 4;
        for (const auto& fix : track.fixes) {
            UtilityEcmwfTracks::Step step;
            step.hour = fix.tau;
            step.lat = fix.lat;
            step.lon = fix.lon;
            step.windLat = fix.lat;
            step.windLon = fix.lon;
            step.wind = fix.wind >= 0 ? fix.wind : missing;
            step.pressure = fix.pressure >= 0 ? fix.pressure : missing;
            m.steps.push_back(step);
        }
        storm.cycle = track.cycle;
        storm.members.push_back(std::move(m));
    }
    return !storm.members.empty();
}
