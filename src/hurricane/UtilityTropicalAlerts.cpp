// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityTropicalAlerts.h"
#include <algorithm>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

string UtilityTropicalAlerts::codeFor(const string& event) {
    if (event == "Hurricane Warning") return "HWR";
    if (event == "Hurricane Watch") return "HWA";
    if (event == "Tropical Storm Warning") return "TWR";
    if (event == "Tropical Storm Watch") return "TWA";
    if (event == "Storm Surge Warning") return "SSW";
    if (event == "Storm Surge Watch") return "SSA";
    return {};
}

int UtilityTropicalAlerts::rank(const string& code) {
    static const char * order[] = {"SSA", "TWA", "HWA", "SSW", "TWR", "HWR"};
    for (int i = 0; i < 6; i++) {
        if (code == order[i]) {
            return i;
        }
    }
    return -1;
}

namespace {
    UtilityTropicalAlerts::Ring ring(const QJsonArray& points) {
        UtilityTropicalAlerts::Ring r;
        for (const auto& p : points) {
            const auto xy = p.toArray();
            if (xy.size() >= 2) {
                r.emplace_back(xy[0].toDouble(), xy[1].toDouble());
            }
        }
        return r;
    }
}

vector<UtilityTropicalAlerts::Area> UtilityTropicalAlerts::parse(const string& geojson) {
    vector<Area> areas;
    const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(geojson));
    for (const auto& value : doc.object().value("features").toArray()) {
        const auto feature = value.toObject();
        const auto properties = feature.value("properties").toObject();
        Area a;
        a.event = properties.value("event").toString().toStdString();
        a.code = codeFor(a.event);
        if (a.code.empty()) {
            continue;
        }
        a.zone = properties.value("areaDesc").toString().toStdString();
        a.office = properties.value("senderName").toString().toStdString();
        a.expires = properties.value("expires").toString().toStdString();
        const auto geometry = feature.value("geometry").toObject();
        const auto type = geometry.value("type").toString();
        const auto coordinates = geometry.value("coordinates").toArray();
        if (type == "Polygon") {
            if (!coordinates.isEmpty()) {
                a.rings.push_back(ring(coordinates[0].toArray()));
            }
        } else if (type == "MultiPolygon") {
            for (const auto& polygon : coordinates) {
                const auto rings = polygon.toArray();
                if (!rings.isEmpty()) {
                    a.rings.push_back(ring(rings[0].toArray()));
                }
            }
        }
        a.rings.erase(std::remove_if(a.rings.begin(), a.rings.end(), [] (const Ring& r) { return r.size() < 3; }), a.rings.end());
        if (a.rings.empty()) {
            continue;
        }
        double lo = 1e9, hi = -1e9, west = 1e9, east = -1e9;
        for (const auto& r : a.rings) {
            for (const auto& [lon, lat] : r) {
                lo = std::min(lo, lat);
                hi = std::max(hi, lat);
                west = std::min(west, lon);
                east = std::max(east, lon);
            }
        }
        a.lat = (lo + hi) / 2.0;
        a.lon = (west + east) / 2.0;
        areas.push_back(std::move(a));
    }
    return areas;
}
