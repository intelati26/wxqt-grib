// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "obs/UtilityMetarHistory.h"
#include <algorithm>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

string UtilityMetarHistory::url(const string& id, int hours) {
    return "https://aviationweather.gov/api/data/metar?ids=" + id + "&format=json&hours=" + std::to_string(hours);
}

vector<UtilityMetarHistory::Ob> UtilityMetarHistory::parse(const string& json) {
    vector<Ob> obs;
    const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    const auto number = [] (const QJsonObject& o, const char * key) {
        const auto v = o.value(key);
        return v.isDouble() ? v.toDouble() : SurfaceStation::missing;
    };
    for (const auto& value : doc.array()) {
        const auto o = value.toObject();
        Ob ob;
        ob.seconds = static_cast<long>(o.value("obsTime").toDouble());
        if (ob.seconds <= 0) {
            continue;
        }
        ob.temperature = number(o, "temp");
        ob.dewPoint = number(o, "dewp");
        ob.windDirection = number(o, "wdir");   // "VRB" is a string, so it stays missing
        ob.windSpeed = number(o, "wspd");
        ob.windGust = number(o, "wgst");
        const double hpa = number(o, "altim");
        if (SurfaceStation::has(hpa)) {
            ob.altimeter = hpa / 33.8639;
        }
        ob.raw = o.value("rawOb").toString().toStdString();
        obs.push_back(std::move(ob));
    }
    std::sort(obs.begin(), obs.end(), [] (const Ob& a, const Ob& b) { return a.seconds < b.seconds; });
    return obs;
}
