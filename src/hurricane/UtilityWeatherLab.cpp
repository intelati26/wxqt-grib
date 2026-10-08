// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityWeatherLab.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>

namespace {
    using Storm = UtilityEcmwfTracks::Storm;

    std::vector<std::string> split(const std::string& line) {
        std::vector<std::string> out;
        size_t from = 0;
        while (true) {
            const auto comma = line.find(',', from);
            if (comma == std::string::npos) {
                out.push_back(line.substr(from));
                return out;
            }
            out.push_back(line.substr(from, comma - from));
            from = comma + 1;
        }
    }

    std::string trim(std::string s) {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n')) {
            s.pop_back();
        }
        size_t i = 0;
        while (i < s.size() && s[i] == ' ') {
            i++;
        }
        return s.substr(i);
    }

    // a number, or the missing value for a blank, "nan" or text
    double number(const std::vector<std::string>& cols, int index) {
        if (index < 0 || static_cast<size_t>(index) >= cols.size()) {
            return UtilityEcmwfTracks::missing;
        }
        const auto text = trim(cols[static_cast<size_t>(index)]);
        if (text.empty()) {
            return UtilityEcmwfTracks::missing;
        }
        char * end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || std::isnan(value)) {
            return UtilityEcmwfTracks::missing;
        }
        return value;
    }
}

std::string UtilityWeatherLab::shortId(const std::string& id) {
    if (id.size() < 8) {
        return id;
    }
    const auto basin = std::string{static_cast<char>(std::toupper(static_cast<unsigned char>(id[0]))), static_cast<char>(std::toupper(static_cast<unsigned char>(id[1])))};
    const char letter = basin == "AL" ? 'L' : basin == "EP" ? 'E' : basin == "CP" ? 'C' : basin == "WP" ? 'W' : basin == "IO" ? 'B' : basin == "SH" ? 'S' : '?';
    return letter == '?' ? id : id.substr(2, 2) + std::string(1, letter);
}

const std::vector<UtilityWeatherLab::Model>& UtilityWeatherLab::models() {
    static const std::vector<Model> list{{"FNV3", "DeepMind FNV3"}, {"WNV3", "DeepMind WNV3"}};
    return list;
}

std::string UtilityWeatherLab::url(const std::string& model, const std::string& cycle) {
    if (cycle.size() != 10) {
        return {};
    }
    return "https://deepmind.google.com/science/weatherlab/download/cyclones/" + model + "/ensemble/paired/csv/" + model + "_" + cycle.substr(0, 4) + "_" + cycle.substr(4, 2) + "_" +
        cycle.substr(6, 2) + "T" + cycle.substr(8, 2) + "_00_paired.csv";
}

bool UtilityWeatherLab::parse(const std::string& csv, std::vector<Storm>& storms, std::string& error) {
    storms.clear();
    std::istringstream in{csv};
    std::string line;
    std::map<std::string, std::string> columnIndexByName;
    int cInit = -1, cTrack = -1, cSample = -1, cLead = -1, cValid = -1, cLat = -1, cLon = -1, cPressure = -1, cWind = -1;
    bool haveHeader = false;
    std::map<std::string, size_t> stormAt;                              // track id -> index in storms
    std::map<std::pair<size_t, int>, size_t> memberAt;                  // (storm, sample) -> index in that storm's members
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#' || trim(line).empty()) {
            continue;
        }
        const auto cols = split(line);
        if (!haveHeader) {
            for (size_t i = 0; i < cols.size(); i++) {
                const auto name = [&] { auto n = trim(cols[i]); std::transform(n.begin(), n.end(), n.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); }); return n; }();
                const int at = static_cast<int>(i);
                if (name == "init_time") cInit = at;
                else if (name == "track_id") cTrack = at;
                else if (name == "sample") cSample = at;
                else if (name == "lead_time_hours") cLead = at;
                else if (name == "valid_time") cValid = at;
                else if (name == "lat" || name == "latitude") cLat = at;
                else if (name == "lon" || name == "longitude") cLon = at;
                else if (name == "minimum_sea_level_pressure_hpa" || name == "mslp") cPressure = at;
                else if (name == "maximum_sustained_wind_speed_knots" || name == "max_wind_kt") cWind = at;
            }
            if (cInit < 0 || cTrack < 0 || cSample < 0 || cLat < 0 || cLon < 0 || (cLead < 0 && cValid < 0)) {
                error = "the Weather Lab file does not have the columns expected (init_time, track_id, sample, lat, lon and the lead time)";
                return false;
            }
            haveHeader = true;
            continue;
        }
        if (static_cast<int>(cols.size()) <= std::max({cInit, cTrack, cSample, cLat, cLon})) {
            continue;
        }
        const auto track = trim(cols[static_cast<size_t>(cTrack)]);
        const int sample = static_cast<int>(std::lround(number(cols, cSample)));
        if (track.empty() || number(cols, cSample) < -1000.0) {
            continue;
        }
        auto found = stormAt.find(track);
        if (found == stormAt.end()) {
            Storm storm;
            storm.id = shortId(track);
            storm.name = track;   // the ATCF id as written ("AL092026"): the short id has lost the year
            const auto init = trim(cols[static_cast<size_t>(cInit)]);   // "2026-10-08 12:00:00"
            if (init.size() >= 13) {
                storm.cycle = init.substr(0, 4) + init.substr(5, 2) + init.substr(8, 2) + init.substr(11, 2);
            }
            storms.push_back(std::move(storm));
            found = stormAt.emplace(track, storms.size() - 1).first;
        }
        auto& storm = storms[found->second];
        auto at = memberAt.find({found->second, sample});
        if (at == memberAt.end()) {
            UtilityEcmwfTracks::Member member;
            member.number = sample;
            member.type = 4;   // every sample is a perturbed member
            storm.members.push_back(std::move(member));
            at = memberAt.emplace(std::make_pair(found->second, sample), storm.members.size() - 1).first;
        }
        UtilityEcmwfTracks::Step step;
        double lead = number(cols, cLead);
        if (lead < -1000.0 && cValid >= 0) {   // no lead column: the difference of the two times
            const auto init = trim(cols[static_cast<size_t>(cInit)]), valid = trim(cols[static_cast<size_t>(cValid)]);
            const auto hours = [] (const std::string& t) {   // "2026-10-08 18:00:00" -> hours from a fixed origin (days are enough to subtract)
                if (t.size() < 13) return -1.0;
                const int y = std::atoi(t.substr(0, 4).c_str()), m = std::atoi(t.substr(5, 2).c_str()), d = std::atoi(t.substr(8, 2).c_str()), h = std::atoi(t.substr(11, 2).c_str());
                const int yy = m <= 2 ? y - 1 : y;
                const long era = (yy >= 0 ? yy : yy - 399) / 400;
                const long yoe = yy - era * 400;
                const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
                const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
                return static_cast<double>((era * 146097 + doe) * 24 + h);
            };
            lead = hours(valid) - hours(init);
        }
        if (lead < 0.0) {
            continue;
        }
        step.hour = static_cast<int>(std::lround(lead));
        step.lat = number(cols, cLat);
        step.lon = number(cols, cLon);
        step.pressure = number(cols, cPressure);
        step.wind = number(cols, cWind);
        step.windLat = step.lat;   // the file gives one position (the minimum pressure)
        step.windLon = step.lon;
        storm.members[at->second].steps.push_back(step);
    }
    if (!haveHeader || storms.empty()) {
        error = "the Weather Lab file has no storms";
        return false;
    }
    for (auto& storm : storms) {
        for (auto& member : storm.members) {
            std::stable_sort(member.steps.begin(), member.steps.end(), [] (const auto& a, const auto& b) { return a.hour < b.hour; });
        }
        std::sort(storm.members.begin(), storm.members.end(), [] (const auto& a, const auto& b) { return a.number < b.number; });
    }
    return true;
}

std::string UtilityWeatherLab::genesisUrl(const std::string& cycle) {
    if (cycle.size() != 10) {
        return {};
    }
    return "https://deepmind.google.com/science/weatherlab/download/cyclones/FNV3_LARGE_ENSEMBLE/ensemble/cyclogenesis/csv/FNV3_LARGE_ENSEMBLE_" + cycle.substr(0, 4) + "_" + cycle.substr(4, 2) + "_" +
        cycle.substr(6, 2) + "T" + cycle.substr(8, 2) + "_00_cyclogenesis.csv";
}

bool UtilityWeatherLab::parseGenesis(std::string_view csv, std::vector<Genesis>& clusters, int& members, std::string& cycle, std::string& error) {
    clusters.clear();
    members = 0;
    cycle.clear();
    std::map<std::string, size_t> clusterAt;
    std::map<std::pair<size_t, int>, size_t> pointAt;   // (cluster, sample) -> index in its points: the earliest row of a member is where it forms
    int cInit = -1, cTrack = -1, cSample = -1, cLead = -1, cLat = -1, cLon = -1, cPressure = -1, cWind = -1;
    bool haveHeader = false;
    size_t at = 0;
    int maxSample = -1;
    while (at < csv.size()) {
        auto end = csv.find('\n', at);
        if (end == std::string_view::npos) {
            end = csv.size();
        }
        const std::string line{csv.substr(at, end - at)};
        at = end + 1;
        if (line.empty() || line[0] == '#' || trim(line).empty()) {
            continue;
        }
        const auto cols = split(line);
        if (!haveHeader) {
            for (size_t i = 0; i < cols.size(); i++) {
                auto name = trim(cols[i]);
                std::transform(name.begin(), name.end(), name.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
                const int index = static_cast<int>(i);
                if (name == "init_time") cInit = index;
                else if (name == "track_id") cTrack = index;
                else if (name == "sample") cSample = index;
                else if (name == "lead_time_hours") cLead = index;
                else if (name == "lat" || name == "latitude") cLat = index;
                else if (name == "lon" || name == "longitude") cLon = index;
                else if (name == "minimum_sea_level_pressure_hpa") cPressure = index;
                else if (name == "maximum_sustained_wind_speed_knots") cWind = index;
            }
            if (cInit < 0 || cTrack < 0 || cSample < 0 || cLead < 0 || cLat < 0 || cLon < 0) {
                error = "the Weather Lab cyclogenesis file does not have the columns expected";
                return false;
            }
            haveHeader = true;
            continue;
        }
        if (static_cast<int>(cols.size()) <= std::max({cInit, cTrack, cSample, cLead, cLat, cLon})) {
            continue;
        }
        const int sample = static_cast<int>(std::lround(number(cols, cSample)));
        maxSample = std::max(maxSample, sample);
        if (cycle.empty()) {
            const auto init = trim(cols[static_cast<size_t>(cInit)]);
            if (init.size() >= 13) {
                cycle = init.substr(0, 4) + init.substr(5, 2) + init.substr(8, 2) + init.substr(11, 2);
            }
        }
        const auto track = trim(cols[static_cast<size_t>(cTrack)]);
        // a storm that exists has an ATCF id (two letters, six digits); a possible new one just a number
        if (track.empty() || !std::all_of(track.begin(), track.end(), [] (unsigned char c) { return std::isdigit(c) != 0; })) {
            continue;
        }
        const int hour = static_cast<int>(std::lround(number(cols, cLead)));
        const double lat = number(cols, cLat), lon = number(cols, cLon);
        if (hour < 0 || lat < -1000.0 || lon < -1000.0) {
            continue;
        }
        auto found = clusterAt.find(track);
        if (found == clusterAt.end()) {
            clusters.push_back({track, {}});
            found = clusterAt.emplace(track, clusters.size() - 1).first;
        }
        auto& cluster = clusters[found->second];
        const auto key = std::make_pair(found->second, sample);
        const auto existing = pointAt.find(key);
        if (existing != pointAt.end() && cluster.points[existing->second].hour <= hour) {
            continue;   // already have this member's earlier row
        }
        GenesisPoint point;
        point.hour = hour;
        point.lat = lat;
        point.lon = lon;
        point.pressure = number(cols, cPressure);
        point.wind = number(cols, cWind);
        if (existing == pointAt.end()) {
            cluster.points.push_back(point);
            pointAt.emplace(key, cluster.points.size() - 1);
        } else {
            cluster.points[existing->second] = point;
        }
    }
    members = maxSample + 1;
    if (!haveHeader || members <= 0) {
        error = "the Weather Lab cyclogenesis file has no members";
        return false;
    }
    return true;
}
