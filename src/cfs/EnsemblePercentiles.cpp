// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/EnsemblePercentiles.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "util/DisplayUnits.h"
#include "util/UtilityIO.h"

float EnsemblePercentiles::percentile(std::vector<float> values, double q) {
    values.erase(std::remove_if(values.begin(), values.end(), [] (float v) { return std::isnan(v); }), values.end());
    if (values.empty()) {
        return std::nanf("");
    }
    std::sort(values.begin(), values.end());
    const double at = std::clamp(q, 0.0, 100.0) / 100.0 * static_cast<double>(values.size() - 1);
    const auto below = static_cast<size_t>(std::floor(at));
    const auto above = std::min(below + 1, values.size() - 1);
    return static_cast<float>(values[below] + (values[above] - values[below]) * (at - static_cast<double>(below)));
}

EnsemblePercentiles::Fan EnsemblePercentiles::fan(const Series& series, const std::vector<int>& levels) {
    Fan out;
    out.times = series.times;
    out.levels = levels;
    for (const int level : levels) {
        out.value[level].assign(series.times.size(), std::nanf(""));
    }
    out.mean.assign(series.times.size(), std::nanf(""));
    out.count.assign(series.times.size(), 0);
    std::vector<float> at;
    for (size_t t = 0; t < series.times.size(); t++) {
        at.clear();
        for (const auto& member : series.members) {
            if (t < member.size() && !std::isnan(member[t])) {
                at.push_back(member[t]);
            }
        }
        out.count[t] = static_cast<int>(at.size());
        if (at.size() < 2) {
            continue;
        }
        double sum = 0.0;
        for (const float v : at) {
            sum += v;
        }
        out.mean[t] = static_cast<float>(sum / static_cast<double>(at.size()));
        for (const int level : levels) {
            out.value[level][t] = percentile(at, level);
        }
    }
    return out;
}

EnsemblePercentiles::Series EnsemblePercentiles::accumulated(const Series& hourly) {
    Series out = hourly;
    for (auto& member : out.members) {
        double total = 0.0;
        bool gone = false;
        for (auto& v : member) {
            if (gone || std::isnan(v)) {
                gone = true;
                v = std::nanf("");
            } else {
                total += static_cast<double>(v);
                v = static_cast<float>(total);
            }
        }
    }
    return out;
}

EnsemblePercentiles::Chance EnsemblePercentiles::chanceOfAtLeast(const Series& hourly, int hours, double threshold) {
    Chance out;
    if (hours < 1 || hourly.times.empty()) {
        return out;
    }
    const long long step = static_cast<long long>(hours) * 3600;
    const auto first = static_cast<long long>(hourly.times.front());
    const auto last = static_cast<long long>(hourly.times.back());
    // blocks [b, b + step) with b a multiple of `step` in UTC; the hourly value at time t is the hour ending at t, so the block holds the values at b + 3600 ... b + step
    for (long long b = (first / step) * step; b + step <= last + 3600; b += step) {
        std::vector<size_t> idx;
        for (size_t t = 0; t < hourly.times.size(); t++) {
            const auto time = static_cast<long long>(hourly.times[t]);
            if (time > b && time <= b + step) {
                idx.push_back(t);
            }
        }
        if (static_cast<int>(idx.size()) != hours) {
            continue;
        }
        std::vector<float> totals;
        for (const auto& member : hourly.members) {
            double sum = 0.0;
            bool missing = false;
            for (const auto t : idx) {
                if (t >= member.size() || std::isnan(member[t])) {
                    missing = true;
                    break;
                }
                sum += static_cast<double>(member[t]);
            }
            if (!missing) {
                totals.push_back(static_cast<float>(sum));
            }
        }
        if (totals.size() < 2) {
            continue;
        }
        const auto atLeast = std::count_if(totals.begin(), totals.end(), [threshold] (float v) { return static_cast<double>(v) >= threshold; });
        out.times.push_back(static_cast<double>(b + step));
        out.share.push_back(static_cast<float>(atLeast) / static_cast<float>(totals.size()));
        out.median.push_back(percentile(totals, 50.0));
        out.high.push_back(percentile(totals, 90.0));
    }
    return out;
}

bool EnsemblePercentiles::parse(const QByteArray& json, const std::string& variable, Series& out, std::string& why) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        why = "the service did not answer with data";
        return false;
    }
    const auto root = doc.object();
    if (root.value("error").toBool()) {
        why = root.value("reason").toString().toStdString();
        return false;
    }
    const auto hourly = root.value("hourly").toObject();
    const auto times = hourly.value("time").toArray();
    if (times.isEmpty()) {
        why = "no hours in the answer";
        return false;
    }
    Series result;
    for (const auto& t : times) {
        result.times.push_back(t.toDouble());
    }
    const auto prefix = QString::fromStdString(variable);
    // the control run (the plain name) and the numbered members: every one is a member of the ensemble
    std::vector<std::pair<QString, std::vector<float>>> columns;
    for (auto it = hourly.begin(); it != hourly.end(); ++it) {
        const auto key = it.key();
        if (key != prefix && !(key.startsWith(prefix + "_member"))) {
            continue;
        }
        std::vector<float> values;
        for (const auto& v : it.value().toArray()) {
            values.push_back(v.isNull() ? std::nanf("") : static_cast<float>(v.toDouble()));
        }
        values.resize(result.times.size(), std::nanf(""));
        columns.emplace_back(key, std::move(values));
    }
    if (columns.size() < 2) {
        why = "the model has fewer than two members for " + variable;
        return false;
    }
    std::sort(columns.begin(), columns.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& c : columns) {
        result.members.push_back(std::move(c.second));
    }
    result.units = root.value("hourly_units").toObject().value(prefix).toString().toStdString();
    out = std::move(result);
    return true;
}

std::string EnsemblePercentiles::url(double lat, double lon, const std::string& model, const std::vector<std::string>& variables, int days) {
    std::string list;
    for (const auto& v : variables) {
        list += (list.empty() ? "" : ",") + v;
    }
    return "https://ensemble-api.open-meteo.com/v1/ensemble?latitude=" + QString::number(lat, 'f', 3).toStdString() + "&longitude=" + QString::number(lon, 'f', 3).toStdString() +
           "&hourly=" + list + "&models=" + model + "&forecast_days=" + std::to_string(days) + "&timeformat=unixtime&wind_speed_unit=ms";
}

const std::vector<EnsemblePercentiles::Model>& EnsemblePercentiles::models() {
    static const std::vector<Model> list{
        {"gfs025", "GEFS (31 members, 16 days)"},
        {"ecmwf_ifs025", "ECMWF IFS ensemble (51 members, 15 days)"},
        {"ecmwf_aifs025", "ECMWF AIFS ensemble (51 members, 15 days)"},
        {"gem_global", "Canadian GEPS (21 members, 16 days)"},
        {"icon_seamless", "DWD ICON ensemble (40 members, 7 days)"},
        {"ukmo_global_ensemble_20km", "UK Met Office ensemble (18 members, 7 days)"},
        {"ncep_aigefs025", "NOAA AIGEFS, the AI ensemble (31 members, 16 days)"},
        {"icon_eu", "DWD ICON-EU ensemble (Europe only, 5 days)"},
    };
    return list;
}

bool EnsemblePercentiles::fetch(double lat, double lon, const std::string& model, Series& temperature, Series& precipitation, std::string& why) {
    static std::mutex lock;
    static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, QByteArray>> kept;
    const auto address = url(lat, lon, model, {"temperature_2m", "precipitation"});
    QByteArray bytes;
    {
        const std::lock_guard<std::mutex> guard{lock};
        const auto found = kept.find(address);
        if (found != kept.end() && std::chrono::steady_clock::now() - found->second.first < std::chrono::minutes{20}) {
            bytes = found->second.second;
        }
    }
    if (bytes.isEmpty()) {
        bytes = UtilityIO::downloadAsByteArray(address);
        if (bytes.contains("Minutely API request limit exceeded")) {
            why = "the service's limit for a minute was reached: try again in a minute";
            return false;
        }
    }
    Series t, p;
    if (!parse(bytes, "temperature_2m", t, why)) {
        return false;
    }
    if (!parse(bytes, "precipitation", p, why)) {
        return false;
    }
    {
        const std::lock_guard<std::mutex> guard{lock};
        kept[address] = {std::chrono::steady_clock::now(), bytes};
    }
    temperature = std::move(t);
    precipitation = std::move(p);
    return true;
}

EnsemblePercentiles::Compare EnsemblePercentiles::compare(const Fan& a, const Fan& b) {
    Compare out;
    std::map<double, size_t> atB;
    for (size_t i = 0; i < b.times.size(); i++) {
        atB[b.times[i]] = i;
    }
    const auto get = [] (const Fan& f, int level, size_t i) { const auto found = f.value.find(level); return found == f.value.end() || i >= found->second.size() ? std::nanf("") : found->second[i]; };
    for (size_t i = 0; i < a.times.size(); i++) {
        const auto found = atB.find(a.times[i]);
        if (found == atB.end()) {
            continue;
        }
        const auto j = found->second;
        out.times.push_back(a.times[i]);
        out.aMedian.push_back(get(a, 50, i));
        out.aLow.push_back(get(a, 25, i));
        out.aHigh.push_back(get(a, 75, i));
        out.bMedian.push_back(get(b, 50, j));
        out.bLow.push_back(get(b, 25, j));
        out.bHigh.push_back(get(b, 75, j));
        out.aP10.push_back(get(a, 10, i));
        out.aP90.push_back(get(a, 90, i));
        out.bP10.push_back(get(b, 10, j));
        out.bP90.push_back(get(b, 90, j));
        out.difference.push_back(out.aMedian.back() - out.bMedian.back());   // (NaN when either is)
        out.meanDifference.push_back(i < a.mean.size() && j < b.mean.size() ? a.mean[i] - b.mean[j] : std::nanf(""));
    }
    // the days that have all 24 hours
    std::map<long long, std::pair<double, int>> sums;   // day -> (sum, hours with a value)
    std::map<long long, int> hours;
    for (size_t i = 0; i < out.times.size(); i++) {
        const auto day = static_cast<long long>(std::floor(out.times[i] / 86400.0)) * 86400;
        hours[day]++;
        if (!std::isnan(out.difference[i])) {
            sums[day].first += static_cast<double>(out.difference[i]);
            sums[day].second++;
        }
    }
    for (const auto& [day, h] : hours) {
        if (h == 24 && sums[day].second == 24) {
            out.dayStart.push_back(static_cast<double>(day));
            out.daily.push_back(static_cast<float>(sums[day].first / 24.0));
        }
    }
    return out;
}

float EnsemblePercentiles::meanDifference(const Compare& c, double days) {
    if (c.times.empty()) {
        return std::nanf("");
    }
    const double end = c.times.front() + days * 86400.0;
    double sum = 0.0;
    int n = 0, all = 0;
    for (size_t i = 0; i < c.times.size() && c.times[i] < end; i++) {
        all++;
        if (!std::isnan(c.difference[i])) {
            sum += static_cast<double>(c.difference[i]);
            n++;
        }
    }
    return n * 2 < all || n == 0 ? std::nanf("") : static_cast<float>(sum / n);
}

std::vector<std::vector<float>> EnsemblePercentiles::onTimes(const Series& series, const std::vector<double>& times) {
    std::map<double, size_t> at;
    for (size_t i = 0; i < series.times.size(); i++) {
        at[series.times[i]] = i;
    }
    std::vector<std::vector<float>> out(series.members.size(), std::vector<float>(times.size(), std::nanf("")));
    for (size_t t = 0; t < times.size(); t++) {
        const auto found = at.find(times[t]);
        if (found == at.end()) {
            continue;
        }
        for (size_t m = 0; m < series.members.size(); m++) {
            if (found->second < series.members[m].size()) {
                out[m][t] = series.members[m][found->second];
            }
        }
    }
    return out;
}

const std::vector<EnsemblePercentiles::Variable>& EnsemblePercentiles::variables() {
    static const std::vector<Variable> list{
        {"temperature_2m", "2 m temperature", "2 m temperature", Quantity::Temperature, false, false, 1},
        {"precipitation", "Precipitation added up", "Precipitation added up since the first hour", Quantity::Precipitation, true, true, 2},
        {"dew_point_2m", "2 m dewpoint", "2 m dewpoint", Quantity::Temperature, false, false, 1},
        {"wind_speed_10m", "10 m wind speed", "10 m wind speed", Quantity::Wind, false, true, 1},
        {"wind_gusts_10m", "10 m wind gusts", "10 m wind gusts (not every model has them)", Quantity::Wind, false, true, 1},
        {"snowfall", "Snowfall added up", "Snowfall added up since the first hour", Quantity::Snow, true, true, 2},
        {"pressure_msl", "Sea level pressure", "Sea level pressure", Quantity::Pressure, false, false, 1},
        {"cloud_cover", "Cloud cover", "Cloud cover", Quantity::Percent, false, true, 0},
        {"relative_humidity_2m", "2 m relative humidity", "2 m relative humidity", Quantity::Percent, false, true, 0},
    };
    return list;
}

const EnsemblePercentiles::Variable * EnsemblePercentiles::findVariable(const std::string& id) {
    for (const auto& v : variables()) {
        if (v.id == id) {
            return &v;
        }
    }
    return nullptr;
}

double EnsemblePercentiles::display(const Variable& variable, double value, bool difference) {
    switch (variable.quantity) {
        case Quantity::Temperature: return DisplayUnits::value(value, "deg C", difference ? DisplayUnits::Kind::Delta : DisplayUnits::Kind::Absolute);
        case Quantity::Precipitation: return DisplayUnits::value(value, "mm");
        case Quantity::Wind: return DisplayUnits::us() ? value * 2.2369363 : value;   // m/s to mph
        case Quantity::Snow: return DisplayUnits::us() ? value / 2.54 : value;       // cm to inches
        default: return value;
    }
}

std::string EnsemblePercentiles::unitLabel(const Variable& variable) {
    switch (variable.quantity) {
        case Quantity::Temperature: return DisplayUnits::label("deg C");
        case Quantity::Precipitation: return DisplayUnits::label("mm");
        case Quantity::Wind: return DisplayUnits::us() ? "mph" : "m/s";
        case Quantity::Snow: return DisplayUnits::us() ? "in" : "cm";
        case Quantity::Pressure: return "hPa";
        default: return "%";
    }
}

bool EnsemblePercentiles::fetchOne(double lat, double lon, const std::string& model, const Variable& variable, Series& out, std::string& why) {
    static std::mutex lock;
    static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, QByteArray>> kept;
    const auto address = url(lat, lon, model, {variable.id});
    QByteArray bytes;
    {
        const std::lock_guard<std::mutex> guard{lock};
        const auto found = kept.find(address);
        if (found != kept.end() && std::chrono::steady_clock::now() - found->second.first < std::chrono::minutes{20}) {
            bytes = found->second.second;
        }
    }
    if (bytes.isEmpty()) {
        bytes = UtilityIO::downloadAsByteArray(address);
        if (bytes.contains("Minutely API request limit exceeded")) {
            why = "the service's limit for a minute was reached: try again in a minute";
            return false;
        }
    }
    Series series;
    if (!parse(bytes, variable.id, series, why)) {
        return false;
    }
    {
        const std::lock_guard<std::mutex> guard{lock};
        kept[address] = {std::chrono::steady_clock::now(), bytes};
    }
    out = variable.accumulate ? accumulated(series) : std::move(series);
    return true;
}
