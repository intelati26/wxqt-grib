// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/UtilityForecastPoint.h"
#include <algorithm>
#include <cmath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <regex>
#include "util/UtilityIO.h"

namespace UtilityForecastPoint {
    const std::vector<Parameter>& parameters() {
        static const std::vector<Parameter> list{
            {"temperature", "Temperature", "F", false},
            {"windChill", "Wind chill", "F", false},
            {"heatIndex", "Heat index", "F", false},
            {"windSpeed", "Wind speed", "mph", false},
            {"windGust", "Wind gust", "mph", false},
            {"probabilityOfPrecipitation", "Chance of precipitation", "%", true},
            {"probabilityOfThunder", "Chance of thunder", "%", true},
            {"quantitativePrecipitation", "Precipitation amount", "in", true},
            {"snowfallAmount", "Snow", "in", true},
            {"iceAccumulation", "Ice", "in", true},
            {"dewpoint", "Dew point", "F", false},
            {"relativeHumidity", "Relative humidity", "%", false},
            {"skyCover", "Sky cover", "%", false},
            {"waveHeight", "Wave height", "ft", false},
        };
        return list;
    }

    long durationHours(const std::string& iso) {
        static const std::regex pattern{"^P(?:([0-9]+)D)?(?:T(?:([0-9]+)H)?(?:([0-9]+)M)?)?$"};
        std::smatch m;
        if (!std::regex_match(iso, m, pattern)) {
            return 1;
        }
        const long days = m[1].matched ? std::stol(m[1]) : 0, hours = m[2].matched ? std::stol(m[2]) : 0, minutes = m[3].matched ? std::stol(m[3]) : 0;
        return std::max(1L, days * 24 + hours + (minutes + 59) / 60);
    }

    std::vector<std::pair<qint64, double>> expand(const std::string& validTime, double value, qint64 horizonEnd) {
        std::vector<std::pair<qint64, double>> out;
        const auto slash = validTime.find('/');
        if (slash == std::string::npos) {
            return out;
        }
        const auto start = QDateTime::fromString(QString::fromStdString(validTime.substr(0, slash)), Qt::ISODate);
        if (!start.isValid()) {
            return out;
        }
        const long hours = durationHours(validTime.substr(slash + 1));
        const qint64 first = start.toSecsSinceEpoch();
        for (long h = 0; h < hours && first + h * 3600 < horizonEnd; h++) {
            out.emplace_back(first + h * 3600, value);
        }
        return out;
    }

    namespace {
        // the unit of a series in the page's: its WMO unit name
        double convert(const std::string& key, const std::string& unit, double v) {
            if (unit.find("degC") != std::string::npos) {
                return v * 9.0 / 5.0 + 32.0;
            }
            if (unit.find("km_h") != std::string::npos) {
                return v * 0.621371;
            }
            if (unit == "wmoUnit:mm") {
                return v / 25.4;
            }
            if (unit == "wmoUnit:m" && key.find("ave") != std::string::npos) {   // wave heights
                return v * 3.28084;
            }
            return v;
        }

        std::string getText(const QUrl& url) {
            return UtilityIO::getHtml(url.toString().toStdString());
        }

        QString query(const std::string& service, int layer, double lat, double lon, const char * field) {
            QUrl url{QString::fromStdString("https://mapservices.weather.noaa.gov/vector/rest/services/" + service + "/MapServer/" + std::to_string(layer) + "/query")};
            QUrlQuery q;
            q.addQueryItem("geometry", QString::number(lon, 'f', 4) + "," + QString::number(lat, 'f', 4));
            q.addQueryItem("geometryType", "esriGeometryPoint");
            q.addQueryItem("inSR", "4326");
            q.addQueryItem("spatialRel", "esriSpatialRelIntersects");
            q.addQueryItem("returnGeometry", "false");
            q.addQueryItem("outFields", QString{field} + ",dn");
            q.addQueryItem("f", "json");
            url.setQuery(q);
            const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(getText(url)));
            const auto features = doc.object().value("features").toArray();
            // the highest category when the point lies in several (they are nested: take the last listed by their rank)
            QString best;
            int rank = -1;
            for (const auto& f : features) {
                const auto attributes = f.toObject().value("attributes").toObject();
                const auto text = attributes.value(field).toString();
                const int r = attributes.contains("dn") ? attributes.value("dn").toInt() : static_cast<int>(text.size());
                if (r > rank) {
                    rank = r;
                    best = text;
                }
            }
            return best;
        }
    }

    std::vector<Day> summarize(const std::map<std::string, Series>& hourly, const QTimeZone& zone, const QDate& first, int count) {
        std::vector<Day> days(static_cast<size_t>(count));
        for (int i = 0; i < count; i++) {
            days[static_cast<size_t>(i)].date = first.addDays(i);
        }
        const auto dayOf = [&] (qint64 t) {
            const auto date = QDateTime::fromSecsSinceEpoch(t, zone).date();
            const auto index = first.daysTo(date);
            return index >= 0 && index < count ? static_cast<int>(index) : -1;
        };
        const auto fold = [&] (const char * key, double Day::* high, double Day::* low) {
            const auto found = hourly.find(key);
            if (found == hourly.end()) {
                return;
            }
            for (const auto& [t, v] : found->second) {
                const int i = dayOf(t);
                if (i < 0) {
                    continue;
                }
                auto& day = days[static_cast<size_t>(i)];
                if (high != nullptr) {
                    day.*high = has(day.*high) ? std::max(day.*high, v) : v;
                }
                if (low != nullptr) {
                    day.*low = has(day.*low) ? std::min(day.*low, v) : v;
                }
            }
        };
        fold("temperature", &Day::maxTemp, &Day::minTemp);
        fold("windChill", nullptr, &Day::minChill);
        fold("heatIndex", &Day::maxHeat, nullptr);
        fold("windSpeed", &Day::maxWind, &Day::minWind);
        fold("windGust", &Day::maxGust, nullptr);
        fold("probabilityOfPrecipitation", &Day::maxPop, nullptr);
        fold("probabilityOfThunder", &Day::maxThunder, nullptr);
        fold("dewpoint", &Day::maxDew, &Day::minDew);
        fold("relativeHumidity", &Day::maxRh, &Day::minRh);
        fold("skyCover", &Day::maxCloud, &Day::minCloud);
        fold("waveHeight", &Day::maxWave, nullptr);
        return days;
    }

    Data fetch(double lat, double lon) {
        Data data;
        data.lat = lat;
        data.lon = lon;
        const auto point = QJsonDocument::fromJson(QByteArray::fromStdString(UtilityIO::getHtml("https://api.weather.gov/points/" + QString::number(lat, 'f', 4).toStdString() + "," + QString::number(lon, 'f', 4).toStdString()))).object().value("properties").toObject();
        data.office = point.value("gridId").toString();
        if (data.office.isEmpty()) {
            data.error = "The NWS has no forecast for this point (it is outside its area).";
            return data;
        }
        const auto relative = point.value("relativeLocation").toObject().value("properties").toObject();
        data.place = relative.value("city").toString() + (relative.value("state").toString().isEmpty() ? QString{} : ", " + relative.value("state").toString());
        data.zone = QTimeZone{point.value("timeZone").toString().toUtf8()};
        if (!data.zone.isValid()) {
            data.zone = QTimeZone::utc();
        }
        const auto grid = QJsonDocument::fromJson(QByteArray::fromStdString(UtilityIO::getHtml("https://api.weather.gov/gridpoints/" + data.office.toStdString() + "/" + std::to_string(point.value("gridX").toInt()) + "," +
                                                                                      std::to_string(point.value("gridY").toInt())))).object().value("properties").toObject();
        if (grid.isEmpty()) {
            data.error = "The forecast data of the point did not arrive.";
            return data;
        }
        data.updated = QDateTime::fromString(grid.value("updateTime").toString(), Qt::ISODate);
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const qint64 horizon = now + 8 * 24 * 3600;
        for (const auto& parameter : parameters()) {
            const auto item = grid.value(parameter.key).toObject();
            const auto unit = item.value("uom").toString().toStdString();
            Series series;
            for (const auto& entry : item.value("values").toArray()) {
                const auto o = entry.toObject();
                if (o.value("value").isNull()) {
                    continue;
                }
                for (auto& [t, v] : expand(o.value("validTime").toString().toStdString(), convert(parameter.key, unit, o.value("value").toDouble()), horizon)) {
                    if (t + 3600 > now - 3600) {
                        series.emplace_back(t, v);
                    }
                }
            }
            if (!series.empty()) {
                data.hourly[parameter.key] = std::move(series);
            }
        }
        const auto today = QDateTime::fromSecsSinceEpoch(now, data.zone).date();
        data.days = summarize(data.hourly, data.zone, today, 7);
        // the outlooks of the point: SPC's categorical outlook (layers 1, 9, 17) and WPC's excessive rainfall outlook (layers 0, 1, 2)
        const int spc[3] = {1, 9, 17};
        for (int i = 0; i < 3; i++) {
            data.severe[i] = query("outlooks/SPC_wx_outlks", spc[i], lat, lon, "label2");
            data.rain[i] = query("hazards/wpc_precip_hazards", i, lat, lon, "outlook");
        }
        data.ok = true;
        return data;
    }
}
