// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityAceForecast.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
    std::vector<std::string> split(const std::string& line) {
        std::vector<std::string> out;
        size_t at = 0;
        while (true) {
            const auto end = line.find(',', at);
            out.push_back(line.substr(at, end == std::string::npos ? std::string::npos : end - at));
            if (end == std::string::npos) {
                break;
            }
            at = end + 1;
        }
        return out;
    }
    std::string trim(std::string s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\n')) s.pop_back();
        size_t a = 0;
        while (a < s.size() && s[a] == ' ') a++;
        return s.substr(a);
    }
}

bool UtilityAceForecast::parse(std::string_view csv, std::vector<Storm>& storms, int& ensembleSize, std::string& cycle, std::string& error) {
    storms.clear();
    ensembleSize = 0;
    cycle.clear();
    std::map<std::string, size_t> at;
    int cInit = -1, cTrack = -1, cSample = -1, cLead = -1, cWind = -1;
    bool haveHeader = false;
    int maxSample = -1;
    size_t pos = 0;
    while (pos < csv.size()) {
        auto end = csv.find('\n', pos);
        if (end == std::string_view::npos) {
            end = csv.size();
        }
        const std::string line{csv.substr(pos, end - pos)};
        pos = end + 1;
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
                else if (name == "maximum_sustained_wind_speed_knots") cWind = index;
            }
            if (cInit < 0 || cTrack < 0 || cSample < 0 || cLead < 0 || cWind < 0) {
                error = "the Weather Lab file does not have the columns expected";
                return false;
            }
            haveHeader = true;
            continue;
        }
        if (static_cast<int>(cols.size()) <= std::max({cInit, cTrack, cSample, cLead, cWind})) {
            continue;
        }
        if (cycle.empty()) {
            const auto init = trim(cols[static_cast<size_t>(cInit)]);
            if (init.size() >= 13) {
                cycle = init.substr(0, 4) + init.substr(5, 2) + init.substr(8, 2) + init.substr(11, 2);
            }
        }
        const int sample = std::atoi(trim(cols[static_cast<size_t>(cSample)]).c_str());
        maxSample = std::max(maxSample, sample);
        const auto track = trim(cols[static_cast<size_t>(cTrack)]);
        if (track.empty()) {
            continue;
        }
        const auto windText = trim(cols[static_cast<size_t>(cWind)]);
        if (windText.empty()) {
            continue;
        }
        const int hour = static_cast<int>(std::lround(std::atof(trim(cols[static_cast<size_t>(cLead)]).c_str())));
        const float wind = static_cast<float>(std::atof(windText.c_str()));
        auto found = at.find(track);
        if (found == at.end()) {
            storms.push_back({track, {}});
            found = at.emplace(track, storms.size() - 1).first;
        }
        auto& member = storms[found->second].members[sample];
        member.hours.push_back(hour);
        member.wind.push_back(wind);
    }
    ensembleSize = maxSample + 1;
    if (!haveHeader || ensembleSize <= 0) {
        error = "the Weather Lab file has no members";
        return false;
    }
    return true;
}

double UtilityAceForecast::percentile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double rank = std::clamp(fraction, 0.0, 1.0) * static_cast<double>(values.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(rank)), hi = static_cast<size_t>(std::ceil(rank));
    return values[lo] + (values[hi] - values[lo]) * (rank - static_cast<double>(lo));
}

UtilityAceForecast::Summary UtilityAceForecast::summarize(const Storm& storm, int ensembleSize) {
    Summary s;
    s.ensembleSize = ensembleSize;
    s.formed = static_cast<int>(storm.members.size());
    s.probability = ensembleSize > 0 ? static_cast<double>(s.formed) / ensembleSize : 0.0;
    int lastHour = 0;
    for (const auto& [number, member] : storm.members) {
        if (!member.hours.empty()) {
            lastHour = std::max(lastHour, member.hours.back());
        }
    }
    const size_t steps = static_cast<size_t>(lastHour / 6) + 1;
    for (size_t i = 0; i < steps; i++) {
        s.hours.push_back(static_cast<int>(i) * 6);
    }
    // each member's running total at every step (flat after its track ends)
    for (const auto& [number, member] : storm.members) {
        std::vector<double> total(steps, 0.0);
        std::vector<double> made(steps, 0.0);
        for (size_t k = 0; k < member.hours.size(); k++) {
            const int hour = member.hours[k];
            if (hour % 6 != 0 || hour < 0) {
                continue;
            }
            const size_t step = static_cast<size_t>(hour / 6);
            if (step < steps && member.wind[k] >= 34.0f) {
                made[step] += static_cast<double>(member.wind[k]) * static_cast<double>(member.wind[k]) / 1.0e4;
            }
        }
        double running = 0.0;
        for (size_t i = 0; i < steps; i++) {
            running += made[i];
            total[i] = running;
        }
        s.lifetime.push_back(running);
        s.cumulative.push_back(std::move(total));
    }
    const size_t members = s.cumulative.size();
    const size_t notFormed = ensembleSize > s.formed ? static_cast<size_t>(ensembleSize - s.formed) : 0;
    for (size_t i = 0; i < steps; i++) {
        std::vector<double> values;
        values.reserve(members);
        double produced = 0.0, sum = 0.0;
        for (const auto& total : s.cumulative) {
            values.push_back(total[i]);
            sum += total[i];
            produced += total[i] - (i > 0 ? total[i - 1] : 0.0);
        }
        s.mean.push_back(members ? sum / static_cast<double>(members) : 0.0);
        s.median.push_back(percentile(values, 0.5));
        s.p10.push_back(percentile(values, 0.1));
        s.p25.push_back(percentile(values, 0.25));
        s.p75.push_back(percentile(values, 0.75));
        s.p90.push_back(percentile(values, 0.9));
        s.minimum.push_back(values.empty() ? 0.0 : *std::min_element(values.begin(), values.end()));
        s.maximum.push_back(values.empty() ? 0.0 : *std::max_element(values.begin(), values.end()));
        s.producedMean.push_back(members ? produced / static_cast<double>(members) : 0.0);
        s.weightedMean.push_back(s.mean.back() * s.probability);
        std::vector<double> all = values;
        all.insert(all.end(), notFormed, 0.0);
        s.weightedMedian.push_back(percentile(all, 0.5));
    }
    return s;
}

std::vector<double> UtilityAceForecast::valuesAt(const Summary& s, size_t step) {
    std::vector<double> out;
    for (const auto& total : s.cumulative) {
        if (step < total.size()) {
            out.push_back(total[step]);
        }
    }
    return out;
}

UtilityAceForecast::Tiles UtilityAceForecast::tilesAt(const Summary& s, size_t step) {
    Tiles t;
    if (step >= s.hours.size()) {
        return t;
    }
    t.valid = true;
    t.maximum = s.maximum[step];
    t.p90 = s.p90[step];
    t.p75 = s.p75[step];
    t.mean = s.mean[step];
    t.median = s.median[step];
    t.weightedMean = s.weightedMean[step];
    t.weightedMedian = s.weightedMedian[step];
    t.p25 = s.p25[step];
    t.p10 = s.p10[step];
    t.minimum = s.minimum[step];
    return t;
}
