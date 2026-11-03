// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/UtilityOpenMeteoPressure.h"
#include <cmath>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "objects/URL.h"

string UtilityOpenMeteoPressure::get(double lat, double lon) {
    const auto url = QString{"https://api.open-meteo.com/v1/forecast?latitude=%1&longitude=%2&hourly=pressure_msl&forecast_days=8&timeformat=unixtime"}
                         .arg(lat, 0, 'f', 3).arg(lon, 0, 'f', 3).toStdString();
    return URL::getText(url);
}

vector<UtilityOpenMeteoPressure::Sample> UtilityOpenMeteoPressure::parse(const string& json, int everyHours) {
    vector<Sample> out;
    if (json.empty()) {
        return out;
    }
    const auto hourly = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object().value("hourly").toObject();
    const auto times = hourly.value("time").toArray();
    const auto pressures = hourly.value("pressure_msl").toArray();
    const long long step = 3600LL * std::max(1, everyHours);
    for (int i = 0; i < times.size() && i < pressures.size(); i++) {
        const long long utc = static_cast<long long>(times[i].toDouble());
        if (utc % step != 0 || pressures[i].isNull() || !std::isfinite(pressures[i].toDouble(std::nan("")))) {
            continue;
        }
        out.push_back({utc, pressures[i].toDouble()});
    }
    return out;
}
