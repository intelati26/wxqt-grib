// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/ModelBias.h"
#include <chrono>
#include <cmath>
#include <mutex>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QTimeZone>
#include "util/UtilityIO.h"

ModelBias::Stat ModelBias::evaluate(const std::vector<double>& times, int utcOffset, const std::vector<float>& forecast, const Truth& truth, bool dailySum) {
    Stat out;
    std::map<double, float> real;
    for (size_t i = 0; i < truth.times.size() && i < truth.values.size(); i++) {
        if (!std::isnan(truth.values[i])) {
            real[truth.times[i]] = truth.values[i];
        }
    }
    struct Day { double sumF{0}, sumO{0}, sumErr{0}; int hours{0}; };
    std::map<long long, Day> days;
    double errSum = 0.0, absSum = 0.0;
    int n = 0;
    for (size_t i = 0; i < times.size() && i < forecast.size(); i++) {
        const auto found = real.find(times[i]);
        if (found == real.end() || std::isnan(forecast[i])) {
            continue;
        }
        const auto day = static_cast<long long>(std::floor((times[i] + utcOffset) / 86400.0)) * 86400 - utcOffset;
        auto& d = days[day];
        const double e = static_cast<double>(forecast[i]) - static_cast<double>(found->second);
        d.sumF += static_cast<double>(forecast[i]);
        d.sumO += static_cast<double>(found->second);
        d.sumErr += e;
        d.hours++;
        if (!dailySum) {
            errSum += e;
            absSum += std::fabs(e);
            n++;
        }
    }
    for (const auto& [day, d] : days) {
        if (dailySum ? d.hours != 24 : d.hours < 20) {
            continue;
        }
        out.dayStart.push_back(static_cast<double>(day));
        out.dayError.push_back(static_cast<float>(dailySum ? d.sumF - d.sumO : d.sumErr / d.hours));
        if (dailySum) {
            errSum += d.sumF - d.sumO;
            absSum += std::fabs(d.sumF - d.sumO);
            n++;
        }
    }
    out.n = n;
    if (n > 0) {
        out.bias = static_cast<float>(errSum / n);
        out.mae = static_cast<float>(absSum / n);
    }
    return out;
}

bool ModelBias::parseForecasts(const QByteArray& json, const std::string& variable, const std::vector<std::string>& models, Forecasts& out, std::string& why) {
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
    Forecasts result;
    result.utcOffset = root.value("utc_offset_seconds").toInt();
    for (const auto& t : times) {
        result.times.push_back(t.toDouble());
    }
    for (const auto& model : models) {
        std::vector<std::vector<float>> leads(8);
        bool any = false;
        for (int k = 0; k < 8; k++) {
            const auto name = QString::fromStdString(variable) + (k == 0 ? "" : QString{"_previous_day%1"}.arg(k)) + "_" + QString::fromStdString(model);
            const auto array = hourly.value(name).toArray();
            leads[static_cast<size_t>(k)].assign(result.times.size(), std::nanf(""));
            for (int i = 0; i < array.size() && static_cast<size_t>(i) < result.times.size(); i++) {
                if (!array[i].isNull()) {
                    leads[static_cast<size_t>(k)][static_cast<size_t>(i)] = static_cast<float>(array[i].toDouble());
                    any = true;
                }
            }
        }
        if (!any) {
            why = model + " has no values for " + variable;
            return false;
        }
        result.byModel[model] = std::move(leads);
    }
    out = std::move(result);
    return true;
}

bool ModelBias::parseTruth(const QByteArray& json, const std::string& variable, Truth& out, std::string& why) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        why = "the archive service did not answer with data";
        return false;
    }
    const auto root = doc.object();
    if (root.value("error").toBool()) {
        why = root.value("reason").toString().toStdString();
        return false;
    }
    const auto hourly = root.value("hourly").toObject();
    const auto times = hourly.value("time").toArray();
    const auto values = hourly.value(QString::fromStdString(variable)).toArray();
    if (times.isEmpty() || values.isEmpty()) {
        why = "the archive has no " + variable;
        return false;
    }
    Truth result;
    result.utcOffset = root.value("utc_offset_seconds").toInt();
    for (int i = 0; i < times.size(); i++) {
        result.times.push_back(times[i].toDouble());
        result.values.push_back(i < values.size() && !values[i].isNull() ? static_cast<float>(values[i].toDouble()) : std::nanf(""));
    }
    out = std::move(result);
    return true;
}

std::string ModelBias::forecastUrl(double lat, double lon, const std::string& variable, const std::vector<std::string>& models, int pastDays) {
    std::string hourly = variable, list;
    for (int k = 1; k <= 7; k++) {
        hourly += "," + variable + "_previous_day" + std::to_string(k);
    }
    for (const auto& m : models) {
        list += (list.empty() ? "" : ",") + m;
    }
    return "https://previous-runs-api.open-meteo.com/v1/forecast?latitude=" + QString::number(lat, 'f', 3).toStdString() + "&longitude=" + QString::number(lon, 'f', 3).toStdString() + "&hourly=" + hourly +
           "&models=" + list + "&past_days=" + std::to_string(pastDays) + "&forecast_days=1&timeformat=unixtime&wind_speed_unit=ms&timezone=auto";
}

std::string ModelBias::truthUrl(double lat, double lon, const std::string& variable, const std::string& from, const std::string& to) {
    return "https://archive-api.open-meteo.com/v1/archive?latitude=" + QString::number(lat, 'f', 3).toStdString() + "&longitude=" + QString::number(lon, 'f', 3).toStdString() + "&start_date=" + from + "&end_date=" + to +
           "&hourly=" + variable + "&timeformat=unixtime&wind_speed_unit=ms&timezone=auto";
}

const std::vector<ModelBias::Model>& ModelBias::models() {
    static const std::vector<Model> list{
        {"ecmwf_ifs025", "ECMWF IFS (the Euro)"},
        {"gfs_global", "NOAA GFS"},
        {"ecmwf_aifs025_single", "ECMWF AIFS (AI)"},
        {"gem_global", "Canadian GDPS"},
        {"icon_global", "DWD ICON"},
        {"ukmo_global_deterministic_10km", "UK Met Office"},
        {"meteofrance_arpege_world", "Meteo-France ARPEGE"},
        {"jma_gsm", "JMA GSM"},
    };
    return list;
}

bool ModelBias::fetch(double lat, double lon, const std::string& variable, const std::string& modelA, const std::string& modelB, int days, Forecasts& forecasts, Truth& truth, std::string& why) {
    static std::mutex lock;
    static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, QByteArray>> kept;
    const auto get = [&] (const std::string& address, QByteArray& bytes) {
        {
            const std::lock_guard<std::mutex> guard{lock};
            const auto found = kept.find(address);
            if (found != kept.end() && std::chrono::steady_clock::now() - found->second.first < std::chrono::minutes{20}) {
                bytes = found->second.second;
                return true;
            }
        }
        bytes = UtilityIO::downloadAsByteArray(address);
        if (bytes.contains("Minutely API request limit exceeded")) {
            why = "the service's limit for a minute was reached: try again in a minute";
            return false;
        }
        return true;
    };
    days = std::max(7, std::min(days, 60));
    const auto today = QDate::currentDate();
    QByteArray f, t;
    const auto fAddress = forecastUrl(lat, lon, variable, {modelA, modelB}, days), tAddress = truthUrl(lat, lon, variable, today.addDays(-days).toString("yyyy-MM-dd").toStdString(), today.toString("yyyy-MM-dd").toStdString());
    if (!get(fAddress, f) || !get(tAddress, t)) {
        return false;
    }
    Forecasts fc;
    Truth tr;
    if (!parseForecasts(f, variable, {modelA, modelB}, fc, why) || !parseTruth(t, variable, tr, why)) {
        return false;
    }
    {
        const std::lock_guard<std::mutex> guard{lock};
        kept[fAddress] = {std::chrono::steady_clock::now(), f};
        kept[tAddress] = {std::chrono::steady_clock::now(), t};
    }
    forecasts = std::move(fc);
    truth = std::move(tr);
    return true;
}

bool ModelBias::parseStationList(const QByteArray& json, double lat, double lon, size_t count, std::vector<Station>& out, std::string& why) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        why = "the station service did not answer with data";
        return false;
    }
    std::vector<Station> found;
    for (const auto& item : doc.object().value("meta").toArray()) {
        const auto o = item.toObject();
        const auto ll = o.value("ll").toArray();
        if (ll.size() != 2 || !o.contains("uid")) {
            continue;
        }
        const double la = ll[1].toDouble(), lo = ll[0].toDouble();
        const double dLat = (la - lat) * M_PI / 180.0, dLon = (lo - lon) * M_PI / 180.0;
        const double a = std::sin(dLat / 2) * std::sin(dLat / 2) + std::cos(lat * M_PI / 180.0) * std::cos(la * M_PI / 180.0) * std::sin(dLon / 2) * std::sin(dLon / 2);
        Station s;
        s.uid = QString::number(o.value("uid").toInt()).toStdString();
        s.name = o.value("name").toString().toStdString();
        s.km = 2.0 * 6371.0 * std::asin(std::min(1.0, std::sqrt(a)));
        found.push_back(s);
    }
    if (found.empty()) {
        why = "no weather station with daily temperatures near this place (the stations are the United States')";
        return false;
    }
    std::sort(found.begin(), found.end(), [] (const Station& x, const Station& y) { return x.km < y.km; });
    if (found.size() > count) {
        found.resize(count);
    }
    out = std::move(found);
    return true;
}

bool ModelBias::parseStationMeta(const QByteArray& json, double lat, double lon, Station& out, std::string& why) {
    std::vector<Station> list;
    if (!parseStationList(json, lat, lon, 1, list, why)) {
        return false;
    }
    out = list.front();
    return true;
}

bool ModelBias::parseStationDaily(const QByteArray& json, int column, std::map<std::string, float>& out, std::string& why) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        why = "the station service did not answer with data";
        return false;
    }
    std::map<std::string, float> days;
    for (const auto& row : doc.object().value("data").toArray()) {
        const auto r = row.toArray();
        if (r.size() <= column) {
            continue;
        }
        const auto text = r[column].toString();
        bool ok = false;
        double v = text.toDouble(&ok);
        if (text == "T") {
            v = 0.0;
            ok = true;
        }
        if (!ok) {
            continue;
        }
        days[r[0].toString().toStdString()] = static_cast<float>(column == 3 ? v * 25.4 : (v - 32.0) / 1.8);
    }
    if (days.empty()) {
        why = "the station has no values for these days";
        return false;
    }
    out = std::move(days);
    return true;
}

ModelBias::Stat ModelBias::evaluateDaily(const std::vector<double>& times, int utcOffset, const std::vector<float>& forecast, const std::map<std::string, float>& station, Judge judge) {
    Stat out;
    struct Day { double high{-1e18}, low{1e18}, sum{0}; int hours{0}; };
    std::map<long long, Day> days;
    for (size_t i = 0; i < times.size() && i < forecast.size(); i++) {
        if (std::isnan(forecast[i])) {
            continue;
        }
        const auto day = static_cast<long long>(std::floor((times[i] + utcOffset) / 86400.0)) * 86400 - utcOffset;
        auto& d = days[day];
        d.high = std::max(d.high, static_cast<double>(forecast[i]));
        d.low = std::min(d.low, static_cast<double>(forecast[i]));
        d.sum += static_cast<double>(forecast[i]);
        d.hours++;
    }
    const auto zone = QTimeZone::fromSecondsAheadOfUtc(utcOffset);
    double errSum = 0.0, absSum = 0.0;
    for (const auto& [day, d] : days) {
        if (judge == Judge::StationRain ? d.hours != 24 : d.hours < 20) {
            continue;
        }
        const auto key = QDateTime::fromSecsSinceEpoch(day + 3600, zone).date().toString("yyyy-MM-dd").toStdString();
        const auto found = station.find(key);
        if (found == station.end()) {
            continue;
        }
        const double f = judge == Judge::StationHigh ? d.high : judge == Judge::StationLow ? d.low : d.sum;
        const double e = f - static_cast<double>(found->second);
        out.dayStart.push_back(static_cast<double>(day));
        out.dayError.push_back(static_cast<float>(e));
        errSum += e;
        absSum += std::fabs(e);
        out.n++;
    }
    if (out.n > 0) {
        out.bias = static_cast<float>(errSum / out.n);
        out.mae = static_cast<float>(absSum / out.n);
    }
    return out;
}

bool ModelBias::fetchStation(double lat, double lon, Judge judge, const std::string& modelA, const std::string& modelB, int days, Forecasts& forecasts, std::map<std::string, float>& station, Station& found, std::string& why) {
    static std::mutex lock;
    static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, QByteArray>> kept;
    const auto get = [&] (const std::string& address, QByteArray& bytes) {
        {
            const std::lock_guard<std::mutex> guard{lock};
            const auto found2 = kept.find(address);
            if (found2 != kept.end() && std::chrono::steady_clock::now() - found2->second.first < std::chrono::minutes{20}) {
                bytes = found2->second.second;
                return true;
            }
        }
        bytes = UtilityIO::downloadAsByteArray(address);
        if (bytes.contains("Minutely API request limit exceeded")) {
            why = "the service's limit for a minute was reached: try again in a minute";
            return false;
        }
        return true;
    };
    days = std::max(7, std::min(days, 60));
    const auto today = QDate::currentDate();
    const auto from = today.addDays(-days).toString("yyyy-MM-dd"), to = today.toString("yyyy-MM-dd");
    const std::string variable = judge == Judge::StationRain ? "precipitation" : "temperature_2m";
    const auto encode = [] (const QJsonObject& o) { return QUrl::toPercentEncoding(QString::fromUtf8(QJsonDocument{o}.toJson(QJsonDocument::Compact))).toStdString(); };
    QJsonObject meta;
    meta["bbox"] = QString{"%1,%2,%3,%4"}.arg(lon - 0.9, 0, 'f', 3).arg(lat - 0.9, 0, 'f', 3).arg(lon + 0.9, 0, 'f', 3).arg(lat + 0.9, 0, 'f', 3);
    meta["elems"] = judge == Judge::StationRain ? "pcpn" : "maxt,mint";
    meta["sdate"] = from;
    meta["edate"] = to;
    meta["meta"] = "name,ll,uid";
    QByteArray metaBytes, fBytes, dataBytes;
    if (!get("https://data.rcc-acis.org/StnMeta?params=" + encode(meta), metaBytes)) {
        return false;
    }
    std::vector<Station> nearby;
    if (!parseStationList(metaBytes, lat, lon, 8, nearby, why)) {
        return false;
    }
    QJsonArray elems;
    for (const char * e : {"maxt", "mint", "pcpn"}) {
        QJsonObject o;
        o["name"] = e;
        elems.append(o);
    }
    const auto fAddress = forecastUrl(lat, lon, variable, {modelA, modelB}, days);
    if (!get(fAddress, fBytes)) {
        return false;
    }
    // the nearest of them that has the days (a station of the list may have none: the list is of stations that ever had them)
    std::map<std::string, float> daily;
    Station nearest;
    bool haveStation = false;
    for (const auto& candidate : nearby) {
        QJsonObject params;
        params["sid"] = QString::fromStdString(candidate.uid);
        params["sdate"] = from;
        params["edate"] = to;
        params["elems"] = elems;
        if (!get("https://data.rcc-acis.org/StnData?params=" + encode(params), dataBytes)) {
            return false;
        }
        std::map<std::string, float> got;
        std::string ignored;
        if (parseStationDaily(dataBytes, judge == Judge::StationHigh ? 1 : judge == Judge::StationLow ? 2 : 3, got, ignored) && static_cast<int>(got.size()) * 10 >= days * 6) {   // 60% of the days
            daily = std::move(got);
            nearest = candidate;
            haveStation = true;
            break;
        }
    }
    if (!haveStation) {
        why = "none of the nearest weather stations has these days (the stations are the United States')";
        return false;
    }
    Forecasts fc;
    if (!parseForecasts(fBytes, variable, {modelA, modelB}, fc, why)) {
        return false;
    }
    {
        const std::lock_guard<std::mutex> guard{lock};
        kept[fAddress] = {std::chrono::steady_clock::now(), fBytes};
    }
    forecasts = std::move(fc);
    station = std::move(daily);
    found = nearest;
    return true;
}

std::map<double, float> ModelBias::dailyOf(const std::vector<double>& times, int utcOffset, const std::vector<float>& values, Aggregate how) {
    struct Day { double high{-1e18}, low{1e18}, sum{0}; int hours{0}; };
    std::map<long long, Day> days;
    for (size_t i = 0; i < times.size() && i < values.size(); i++) {
        if (std::isnan(values[i])) {
            continue;
        }
        const auto day = static_cast<long long>(std::floor((times[i] + utcOffset) / 86400.0)) * 86400 - utcOffset;
        auto& d = days[day];
        d.high = std::max(d.high, static_cast<double>(values[i]));
        d.low = std::min(d.low, static_cast<double>(values[i]));
        d.sum += static_cast<double>(values[i]);
        d.hours++;
    }
    std::map<double, float> out;
    for (const auto& [day, d] : days) {
        if (how == Aggregate::Sum ? d.hours != 24 : d.hours < 20) {
            continue;
        }
        out[static_cast<double>(day)] = static_cast<float>(how == Aggregate::High ? d.high : how == Aggregate::Low ? d.low : how == Aggregate::Sum ? d.sum : d.sum / d.hours);
    }
    return out;
}
