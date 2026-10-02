// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "radar/HistoricalWarnings.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include "util/UtilityIO.h"

namespace HistoricalWarnings {
    namespace {
        struct Entry {
            string phenomena;       // "TO"
            string significance;    // "W"
            vector<vector<LatLon>> outlines;
        };
        using Entries = vector<Entry>;

        std::mutex cacheMutex;
        std::map<string, std::shared_ptr<Entries>> cache;   // by the minute asked for; a few kept

        // GeoJSON ring [[lon, lat], ...] as the app's lat / lon (longitude positive west)
        vector<LatLon> ringOf(const QJsonArray& ring) {
            vector<LatLon> out;
            for (const auto& point : ring) {
                const auto pair = point.toArray();
                if (pair.size() >= 2) {
                    out.emplace_back(pair.at(1).toDouble(), -pair.at(0).toDouble());
                }
            }
            return out;
        }

        std::shared_ptr<Entries> load(const QDateTime& utc) {
            const auto stamp = utc.toUTC().toString("yyyy-MM-dd'T'HH:mm:00'Z'").toStdString();
            {
                const std::lock_guard<std::mutex> lock{cacheMutex};
                const auto found = cache.find(stamp);
                if (found != cache.end()) {
                    return found->second;
                }
            }
            auto entries = std::make_shared<Entries>();
            const auto text = UtilityIO::getHtml("https://mesonet.agron.iastate.edu/geojson/sbw.py?ts=" + stamp);
            const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(text));
            for (const auto& featureValue : document.object().value("features").toArray()) {
                const auto feature = featureValue.toObject();
                const auto properties = feature.value("properties").toObject();
                const auto geometry = feature.value("geometry").toObject();
                Entry entry;
                entry.phenomena = properties.value("phenomena").toString().toStdString();
                entry.significance = properties.value("significance").toString().toStdString();
                const auto type = geometry.value("type").toString();
                const auto coordinates = geometry.value("coordinates").toArray();
                if (type == "Polygon") {
                    if (!coordinates.isEmpty()) {
                        entry.outlines.push_back(ringOf(coordinates.at(0).toArray()));   // the outer ring
                    }
                } else if (type == "MultiPolygon") {
                    for (const auto& polygon : coordinates) {
                        const auto rings = polygon.toArray();
                        if (!rings.isEmpty()) {
                            entry.outlines.push_back(ringOf(rings.at(0).toArray()));
                        }
                    }
                }
                if (!entry.outlines.empty()) {
                    entries->push_back(std::move(entry));
                }
            }
            if (text.empty()) {
                return entries;   // a failed download is not remembered
            }
            const std::lock_guard<std::mutex> lock{cacheMutex};
            if (cache.size() > 64) {   // a loop's frames are all kept
                cache.clear();
            }
            cache[stamp] = entries;
            return entries;
        }

        // the archive's code of each warning type the radar draws ("" = none in the archive)
        string phenomenaOf(PolygonType type) {
            switch (type) {
                case Tor: return "TO";
                case Tst: return "SV";
                case Ffw: return "FF";
                case Smw: return "MA";
                case Sqw: return "SQ";
                case Dsw: return "DS";
                default: return "";
            }
        }
    }

    vector<vector<LatLon>> polygonsAt(PolygonType type, const QDateTime& utc) {
        vector<vector<LatLon>> out;
        const auto code = phenomenaOf(type);
        if (code.empty() || !utc.isValid()) {
            return out;
        }
        for (const auto& entry : *load(utc)) {
            if (entry.phenomena == code && entry.significance == "W") {
                for (const auto& outline : entry.outlines) {
                    out.push_back(outline);
                }
            }
        }
        return out;
    }
}
