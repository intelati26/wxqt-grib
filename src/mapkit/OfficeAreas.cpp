// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/OfficeAreas.h"
#include <algorithm>
#include <mutex>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "objects/URL.h"
#include "util/PermanentCache.h"

namespace {
    std::vector<OfficeAreas::Office> parse(const std::string& text) {
        std::vector<OfficeAreas::Office> out;
        const auto features = QJsonDocument::fromJson(QByteArray::fromStdString(text)).object().value("features").toArray();
        for (const auto& value : features) {
            const auto feature = value.toObject();
            const auto properties = feature.value("properties").toObject();
            const auto geometry = feature.value("geometry").toObject();
            OfficeAreas::Office office;
            office.id = properties.value("cwa").toString().toStdString();
            const auto city = properties.value("city").toString(), state = properties.value("st").toString();
            office.name = (city + (state.isEmpty() ? QString{} : ", " + state)).toStdString();
            const auto ringsOf = [&office] (const QJsonArray& polygon) {   // a polygon: its outer ring first, the holes after (an office's area has none that matter)
                if (polygon.isEmpty()) {
                    return;
                }
                QPolygonF ring;
                for (const auto& pointValue : polygon.first().toArray()) {
                    const auto point = pointValue.toArray();
                    ring << QPointF{point.at(0).toDouble(), point.at(1).toDouble()};
                }
                office.rings.push_back(ring);
            };
            const auto type = geometry.value("type").toString();
            if (type == "Polygon") {
                ringsOf(geometry.value("coordinates").toArray());
            } else if (type == "MultiPolygon") {
                for (const auto& polygon : geometry.value("coordinates").toArray()) {
                    ringsOf(polygon.toArray());
                }
            }
            if (!office.id.empty() && !office.rings.empty()) {
                out.push_back(std::move(office));
            }
        }
        std::sort(out.begin(), out.end(), [] (const auto& a, const auto& b) { return a.id < b.id; });
        return out;
    }
}

const std::vector<OfficeAreas::Office>& OfficeAreas::all(std::string& problem) {
    static std::mutex mutex;
    static std::vector<Office> kept;
    const std::lock_guard lock{mutex};
    if (!kept.empty()) {
        return kept;
    }
    const PermanentCache store{"cwa"};
    auto text = store.read("cwa-0p02.json");
    if (text.empty()) {
        text = URL::getText("https://mapservices.weather.noaa.gov/static/rest/services/nws_reference_maps/nws_reference_map/MapServer/1/query?where=1%3D1&outFields=cwa,city,st&returnGeometry=true&outSR=4326&maxAllowableOffset=0.02&f=geojson");
        auto fresh = parse(text);
        if (fresh.size() < 50) {
            problem = "the forecast office areas could not be read from the NWS map service";
            return kept;
        }
        store.write("cwa-0p02.json", text);
        kept = std::move(fresh);
        return kept;
    }
    kept = parse(text);
    if (kept.empty()) {
        problem = "the kept forecast office areas could not be read";
    }
    return kept;
}
