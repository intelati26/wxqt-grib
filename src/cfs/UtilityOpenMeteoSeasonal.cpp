// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/UtilityOpenMeteoSeasonal.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <thread>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "gfs/GfsCache.h"
#include "util/UtilityIO.h"

namespace {
    // a request is asked again after `minutes` at the earliest (the forecasts change daily at most): kept in memory and in a file, so that a program started again does not spend the service's quota
    // (about 600 points a minute) on what it has just read
    QByteArray fetch(const std::string& url, int minutes = 30) {
        static std::mutex lock;
        static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, QByteArray>> kept;
        {
            std::lock_guard<std::mutex> guard{lock};
            const auto found = kept.find(url);
            if (found != kept.end() && std::chrono::steady_clock::now() - found->second.first < std::chrono::minutes{minutes}) {
                return found->second.second;
            }
        }
        const QString path = GfsCache::folder() + "/open-meteo/" + QString::fromLatin1(QCryptographicHash::hash(QByteArray::fromStdString(url), QCryptographicHash::Sha1).toHex());
        {
            QFileInfo info{path};
            if (info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < minutes * 60) {
                QFile file{path};
                if (file.open(QIODevice::ReadOnly)) {
                    const auto bytes = file.readAll();
                    if (!bytes.isEmpty() && !bytes.contains("\"error\":true")) {
                        std::lock_guard<std::mutex> guard{lock};
                        kept[url] = {std::chrono::steady_clock::now(), bytes};
                        return bytes;
                    }
                }
            }
        }
        auto bytes = UtilityIO::downloadAsByteArray(url);
        if (minutes >= 360 && bytes.contains("Minutely API request limit exceeded")) {   // a map's points count against the service's minute: wait it out once
            std::this_thread::sleep_for(std::chrono::seconds{62});
            bytes = UtilityIO::downloadAsByteArray(url);
        }
        if (!bytes.isEmpty() && !bytes.contains("\"error\":true")) {   // (an answer that says the limit is reached is not kept)
            std::lock_guard<std::mutex> guard{lock};
            kept[url] = {std::chrono::steady_clock::now(), bytes};
            QDir{}.mkpath(QFileInfo{path}.absolutePath());
            QFile file{path};
            if (file.open(QIODevice::WriteOnly)) {
                file.write(bytes);
            }
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

bool UtilityOpenMeteoSeasonal::periodGrid(bool weekly, const std::string& variable, double west, double south, double east, double north, double step, PeriodGrid& out, std::string& error) {
    out = {};
    out.step = step;
    out.west = west;
    out.north = north;
    out.columns = std::max(1, static_cast<int>(std::floor((east - west) / step)) + 1);
    out.rows = std::max(1, static_cast<int>(std::floor((north - south) / step)) + 1);
    const size_t points = static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows);
    if (points > 1200) {
        error = "too many points for one map (" + std::to_string(points) + ")";
        return false;
    }
    std::vector<double> lats(points), lons(points);
    for (int r = 0; r < out.rows; r++) {
        for (int c = 0; c < out.columns; c++) {
            const size_t i = static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c);
            lats[i] = std::clamp(north - r * step, -89.0, 89.0);
            lons[i] = std::clamp(west + c * step, -180.0, 180.0);
        }
    }
    constexpr size_t chunk = 250;
    for (size_t from = 0; from < points; from += chunk) {
        const size_t to = std::min(points, from + chunk);
        QString la, lo;
        for (size_t i = from; i < to; i++) {
            la += (i > from ? "," : "") + QString::number(lats[i], 'f', 2);
            lo += (i > from ? "," : "") + QString::number(lons[i], 'f', 2);
        }
        const auto bytes = fetch("https://seasonal-api.open-meteo.com/v1/seasonal?latitude=" + la.toStdString() + "&longitude=" + lo.toStdString() + (weekly ? "&weekly=" : "&monthly=") + variable + "&timezone=UTC", 360);
        const auto document = QJsonDocument::fromJson(bytes);
        QJsonArray answers;
        if (document.isArray()) {
            answers = document.array();
        } else if (document.isObject() && !document.object().value("error").toBool()) {
            answers.append(document.object());   // (one point: an object, not a list)
        } else {
            error = bytes.isEmpty() ? std::string{"Open-Meteo did not answer"} : "Open-Meteo: " + document.object().value("reason").toString().toStdString();
            return false;
        }
        for (int k = 0; k < answers.size() && from + static_cast<size_t>(k) < to; k++) {
            const auto block = answers.at(k).toObject().value(weekly ? "weekly" : "monthly").toObject();
            const auto times = block.value("time").toArray();
            const auto values = numbers(block.value(QString::fromStdString(variable)), static_cast<size_t>(times.size()));
            if (out.starts.empty()) {
                for (int i = 0; i < times.size(); i++) {
                    out.starts.push_back(QDate::fromString(times.at(i).toString(), Qt::ISODate));
                }
                out.values.assign(out.starts.size(), std::vector<float>(points, std::nanf("")));
            }
            for (size_t w = 0; w < out.starts.size() && w < values.size(); w++) {
                out.values[w][from + static_cast<size_t>(k)] = values[w];
            }
        }
    }
    if (out.starts.empty()) {
        error = "no values for these periods";
        return false;
    }
    // only the weeks the model reaches
    size_t keep = 0;
    for (size_t w = 0; w < out.values.size(); w++) {
        if (std::any_of(out.values[w].begin(), out.values[w].end(), [] (float v) { return !std::isnan(v); })) {
            keep = w + 1;
        }
    }
    out.starts.resize(keep);
    out.values.resize(keep);
    return keep > 0;
}
