// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/OfficeAreas.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "objects/URL.h"
#include "util/PermanentCache.h"

namespace {
    std::vector<OfficeAreas::Office> parse(const std::string& text, const std::string& kind) {
        std::vector<OfficeAreas::Office> out;
        const auto features = QJsonDocument::fromJson(QByteArray::fromStdString(text)).object().value("features").toArray();
        for (const auto& value : features) {
            const auto feature = value.toObject();
            const auto properties = feature.value("properties").toObject();
            const auto geometry = feature.value("geometry").toObject();
            OfficeAreas::Office office;
            if (kind == "rfc") {
                office.id = properties.value("basin_id").toString().toStdString();
                office.name = (properties.value("rfc_name").toString() + " (" + properties.value("rfc_city").toString() + ")").toStdString();
            } else if (kind == "state") {
                office.id = properties.value("state").toString().toStdString();
                office.name = properties.value("name").toString().toStdString();
            } else {
                office.id = properties.value("cwa").toString().toStdString();
                const auto city = properties.value("city").toString(), state = properties.value("st").toString();
                office.name = (city + (state.isEmpty() ? QString{} : ", " + state)).toStdString();
            }
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

const std::vector<OfficeAreas::Office>& OfficeAreas::all(std::string& problem, const std::string& kind) {
    static std::mutex mutex;
    static std::map<std::string, std::vector<Office>> kept;
    const std::lock_guard lock{mutex};
    auto& list = kept[kind];
    if (!list.empty()) {
        return list;
    }
    const int layer = kind == "rfc" ? 11 : kind == "state" ? 3 : 1;
    const std::string fields = kind == "rfc" ? "basin_id,rfc_name,rfc_city" : kind == "state" ? "state,name" : "cwa,city,st";
    const PermanentCache store{"cwa"};
    const std::string name = kind + "-0p02.json";
    auto text = store.read(name);
    if (text.empty()) {
        text = URL::getText("https://mapservices.weather.noaa.gov/static/rest/services/nws_reference_maps/nws_reference_map/MapServer/" + std::to_string(layer) + "/query?where=1%3D1&outFields=" + fields +
                            "&returnGeometry=true&outSR=4326&maxAllowableOffset=0.02&f=geojson");
        auto fresh = parse(text, kind);
        if (fresh.size() < 5) {
            problem = "the " + kind + " areas could not be read from the NWS map service";
            return list;
        }
        store.write(name, text);
        list = std::move(fresh);
        return list;
    }
    list = parse(text, kind);
    if (list.empty()) {
        problem = "the kept " + kind + " areas could not be read";
    }
    return list;
}

const OfficeAreas::Office * OfficeAreas::find(const std::string& full, std::string& label) {
    std::string kind = "cwa", id = full;
    if (const auto colon = full.find(':'); colon != std::string::npos) {
        const auto prefix = full.substr(0, colon);
        kind = prefix == "RFC" ? "rfc" : prefix == "ST" ? "state" : "cwa";
        id = full.substr(colon + 1);
    }
    std::string problem;
    for (const auto& o : all(problem, kind)) {
        if (o.id == id) {
            label = o.id + " " + o.name;
            return &o;
        }
    }
    return nullptr;
}
