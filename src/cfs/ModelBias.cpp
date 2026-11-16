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
