// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/UtilityOpenMeteoSeasonal.h"
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "util/UtilityIO.h"

namespace {
    // a request is asked again after half an hour at the earliest (the forecasts change daily at most)
    QByteArray fetch(const std::string& url) {
        static std::mutex lock;
        static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, QByteArray>> kept;
        {
            std::lock_guard<std::mutex> guard{lock};
            const auto found = kept.find(url);
            if (found != kept.end() && std::chrono::steady_clock::now() - found->second.first < std::chrono::minutes{30}) {
                return found->second.second;
            }
        }
        const auto bytes = UtilityIO::downloadAsByteArray(url);
        if (!bytes.isEmpty()) {
            std::lock_guard<std::mutex> guard{lock};
            kept[url] = {std::chrono::steady_clock::now(), bytes};
        }
        return bytes;
    }

    std::string base(double lat, double lon) {
        return "https://seasonal-api.open-meteo.com/v1/seasonal?latitude=" + QString::number(lat, 'f', 3).toStdString() + "&longitude=" + QString::number(lon, 'f', 3).toStdString() + "&timezone=UTC";
    }

    // the object of the reply, or the reason it is none
    bool parse(const QByteArray& bytes, const char * block, QJsonObject& out, std::string& error) {
        if (bytes.isEmpty()) {
            error = "Open-Meteo did not answer";
            return false;
        }
        const auto document = QJsonDocument::fromJson(bytes);
        const auto root = document.object();
        if (root.value("error").toBool()) {
            error = "Open-Meteo: " + root.value("reason").toString().toStdString();
            return false;
        }
        if (!root.value(block).isObject()) {
            error = "Open-Meteo sent no " + std::string{block} + " values";
            return false;
        }
        out = root.value(block).toObject();
        return true;
    }

    std::vector<float> numbers(const QJsonValue& value, size_t n) {
        std::vector<float> out(n, std::nanf(""));
        const auto array = value.toArray();
        for (int i = 0; i < array.size() && static_cast<size_t>(i) < n; i++) {
            if (array.at(i).isDouble()) {
                out[static_cast<size_t>(i)] = static_cast<float>(array.at(i).toDouble());
            }
        }
        return out;
    }
}

const std::vector<UtilityOpenMeteoSeasonal::Variable>& UtilityOpenMeteoSeasonal::variables() {
    static const std::vector<Variable> list{
        {"temperature_2m_mean", "2 m temperature (daily mean)", "deg C", "temperature_2m_mean", "temperature_2m_anomaly", true},
        {"temperature_2m_max", "2 m maximum temperature", "deg C", "", "", true},
        {"temperature_2m_min", "2 m minimum temperature", "deg C", "", "", true},
        {"precipitation_sum", "precipitation (daily total)", "mm", "precipitation_mean", "precipitation_anomaly", false},
        {"snowfall_sum", "snowfall (daily total)", "cm", "", "", false},
        {"pressure_msl_mean", "sea level pressure", "hPa", "pressure_msl_mean", "pressure_msl_anomaly", true},
        {"cloud_cover_mean", "cloud cover", "%", "cloud_cover_mean", "", false},
        {"relative_humidity_2m_mean", "2 m relative humidity", "%", "", "", false},
    };
    return list;
}

const UtilityOpenMeteoSeasonal::Variable * UtilityOpenMeteoSeasonal::find(const std::string& key) {
    for (const auto& v : variables()) {
        if (v.key == key) {
            return &v;
        }
    }
    return nullptr;
}

bool UtilityOpenMeteoSeasonal::daily(const std::string& model, const std::string& variable, double lat, double lon, Daily& out, std::string& error) {
    QJsonObject block;
    if (!parse(fetch(base(lat, lon) + "&models=" + model + "&daily=" + variable), "daily", block, error)) {
        return false;
    }
    const auto times = block.value("time").toArray();
    const size_t n = static_cast<size_t>(times.size());
    out = {};
    for (int i = 0; i < times.size(); i++) {
        out.days.push_back(QDate::fromString(times.at(i).toString(), Qt::ISODate));
    }
    out.members.push_back(numbers(block.value(QString::fromStdString(variable)), n));   // the control
    for (int m = 1; m <= 50; m++) {
        const auto name = QString::fromStdString(variable) + "_member" + QString{"%1"}.arg(m, 2, 10, QChar{'0'});
        if (block.contains(name)) {
            out.members.push_back(numbers(block.value(name), n));
        }
    }
    out.mean.assign(n, std::nanf(""));
    for (size_t i = 0; i < n; i++) {
        double sum = 0.0;
        int count = 0;
        for (const auto& member : out.members) {
            if (!std::isnan(member[i])) {
                sum += member[i];
                count++;
            }
        }
        if (count > 0) {
            out.mean[i] = static_cast<float>(sum / count);
        }
    }
    if (out.members.front().empty() || out.days.empty()) {
        error = "no values for this place";
        return false;
    }
    out.attribution = "Weather data by Open-Meteo.com, based on ECMWF data (CC BY 4.0)";
    return true;
}

bool UtilityOpenMeteoSeasonal::periods(bool weekly, const std::string& variable, double lat, double lon, Periods& out, std::string& error) {
    QJsonObject block;
    if (!parse(fetch(base(lat, lon) + (weekly ? "&weekly=" : "&monthly=") + variable), weekly ? "weekly" : "monthly", block, error)) {
        return false;
    }
    const auto times = block.value("time").toArray();
    const auto values = numbers(block.value(QString::fromStdString(variable)), static_cast<size_t>(times.size()));
    out = {};
    for (int i = 0; i < times.size(); i++) {
        if (!std::isnan(values[static_cast<size_t>(i)])) {
            out.starts.push_back(QDate::fromString(times.at(i).toString(), Qt::ISODate));
            out.values.push_back(values[static_cast<size_t>(i)]);
        }
    }
    return !out.values.empty();
}
