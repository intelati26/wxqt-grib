// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/SevereAreasLayers.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <QCheckBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainterPath>
#include <QPolygonF>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>
#include "misc/TextViewerStatic.h"
#include "objects/FutureBytes.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

namespace {
    using SevereAreas::Area;

    // the kinds of warning polygon (bits of the mask)
    enum Kind { Tornado = 0, Thunderstorm = 1, FlashFlood = 2, Flood = 3, Marine = 4, Other = 5, TornadoWatch = 6, StormWatch = 7, Discussion = 8 };
    const char * const kindNames[] = {"Tornado warning", "Severe thunderstorm warning", "Flash flood warning", "Flood warning / advisory", "Special marine warning", "Other warnings", "Tornado watch", "Severe thunderstorm watch", "Mesoscale discussion"};

    QColor colorOf(int kind) {
        switch (kind) {
            case Tornado: return QColor{255, 20, 20};
            case Thunderstorm: return QColor{255, 165, 0};
            case FlashFlood: return QColor{150, 0, 20};
            case Flood: return QColor{0, 190, 90};
            case Marine: return QColor{255, 120, 40};
            case TornadoWatch: return QColor{255, 235, 0};
            case StormWatch: return QColor{230, 110, 160};
            case Discussion: return QColor{40, 150, 255};
            default: return QColor{130, 130, 200};
        }
    }

    // a GeoJSON geometry's outer rings
    void rings(const QJsonObject& geometry, std::vector<std::vector<QPointF>>& out) {
        const auto type = geometry.value("type").toString();
        const auto ring = [&out] (const QJsonArray& coordinates) {
            std::vector<QPointF> r;
            for (const auto& c : coordinates) {
                const auto pair = c.toArray();
                if (pair.size() >= 2) {
                    r.emplace_back(pair.at(0).toDouble(), pair.at(1).toDouble());
                }
            }
            if (r.size() >= 3) {
                out.push_back(std::move(r));
            }
        };
        const auto coordinates = geometry.value("coordinates").toArray();
        if (type == "Polygon" && !coordinates.isEmpty()) {
            ring(coordinates.at(0).toArray());
        } else if (type == "MultiPolygon") {
            for (const auto& polygon : coordinates) {
                const auto parts = polygon.toArray();
                if (!parts.isEmpty()) {
                    ring(parts.at(0).toArray());
                }
            }
        }
    }

    long secondsOf(const QString& iso) {
        auto t = QDateTime::fromString(iso, Qt::ISODate);
        return t.isValid() ? static_cast<long>(t.toSecsSinceEpoch()) : 0;
    }

    QString zulu(long seconds) {
        return seconds == 0 ? QString{"?"} : QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).toString("ddd d HH:mm") + "Z";
    }

    void paintAreas(QPainter& painter, MapHost& host, const std::vector<Area>& areas, const std::function<bool(const Area&)>& shown, const std::vector<int>& order) {
        const auto t = host.view().transform();
        const double px = host.view().unitsPerPixel();
        for (const int kind : order) {
            for (const auto& a : areas) {
                if (a.kind != kind || !shown(a)) {
                    continue;
                }
                const auto color = colorOf(kind);
                const bool watch = kind == TornadoWatch || kind == StormWatch || kind == Discussion;
                painter.setPen(QPen{a.strong ? QColor{255, 0, 255} : color, (watch ? 2.4 : (a.strong ? 3.0 : 2.0)) * px, kind == Discussion ? Qt::DashLine : Qt::SolidLine});
                painter.setBrush(watch ? QBrush{Qt::NoBrush} : QBrush{QColor{color.red(), color.green(), color.blue(), 45}});
                for (const auto& r : a.rings) {
                    QPolygonF polygon;
                    for (const auto& p : r) {
                        polygon << t(p.y(), p.x());
                    }
                    painter.drawPolygon(polygon);
                }
            }
        }
    }

    // the areas that contain the pointer (view pixels)
    MapHit pickAreas(const QPointF& pixels, MapHost& host, const std::vector<Area>& areas, const std::function<bool(const Area&)>& shown, int priority) {
        MapHit hit;
        hit.distance = 1e9;
        hit.priority = priority;
        QStringList texts;
        const Area * first = nullptr;
        for (const auto& a : areas) {
            if (!shown(a)) {
                continue;
            }
            for (const auto& r : a.rings) {
                QPolygonF polygon;
                for (const auto& p : r) {
                    polygon << host.view().toPixels(p.y(), p.x());
                }
                if (polygon.containsPoint(pixels, Qt::OddEvenFill)) {
                    texts << a.text;
                    if (first == nullptr && !a.textUrl.isEmpty()) {
                        first = &a;
                    }
                    break;
                }
            }
            if (texts.size() >= 4) {
                break;
            }
        }
        if (texts.isEmpty()) {
            return hit;
        }
        hit.distance = 0.0;
        hit.reach = 1.0;
        hit.text = texts.join("\n\n");
        if (first != nullptr) {
            const auto url = first->textUrl.toStdString();
            const auto title = first->windowTitle.toStdString();
            hit.text += "\n(click for the text)";
            hit.open = [url, title] (Window * parent) {
                new FutureBytes{parent, url, [parent, title] (const QByteArray& bytes) {
                    new TextViewerStatic{parent, bytes.isEmpty() ? string{"The text could not be read."} : bytes.toStdString(), title, 800, 700};
                }};
            };
        }
        return hit;
    }

    vector<MapLegendRow> legendOf(const std::vector<int>& kinds, const QString& title) {
        MapLegendRow row;
        row.title = title;
        for (const int k : kinds) {
            row.entries.push_back({MapLegendEntry::Square, colorOf(k), kindNames[k]});
        }
        return {row};
    }
}

// ---- the warnings and watches -------------------------------------------------------------------------------------------------------------------------------------------------------------------

WarningPolygonsLayer::WarningPolygonsLayer() {
    const auto saved = Utility::readPref("MM_WARNING_KINDS", "");
    if (!saved.empty()) {
        enabledMask = std::atoi(saved.c_str());
    }
}

bool WarningPolygonsLayer::wanted(int kind) const {
    return (enabledMask & (1 << kind)) != 0;
}

string WarningPolygonsLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (loading && areas.empty()) {
        return "reading the warnings...";
    }
    int count[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (const auto& a : areas) {
        if (a.kind >= 0 && a.kind < 8) {
            count[a.kind]++;
        }
    }
    return std::to_string(count[Tornado]) + " tornado, " + std::to_string(count[Thunderstorm]) + " severe tstm, " + std::to_string(count[FlashFlood]) + " flash flood, " + std::to_string(count[TornadoWatch] + count[StormWatch]) + " watch parts" +
           (asOf.isValid() && asOf.secsTo(QDateTime::currentDateTimeUtc()) > 15 * 60 ? " (as of " + asOf.toString("yyyy-MM-dd HH:mm").toStdString() + "Z)" : std::string{});
}

void WarningPolygonsLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    generation++;
    areas.clear();
    refresh(host);
}

void WarningPolygonsLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const int mine = generation;
    const bool past = asOf.isValid() && asOf.secsTo(QDateTime::currentDateTimeUtc()) > 15 * 60;   // (a time within a quarter hour of now is the live feed)
    const string warningsUrl = past ? "https://mesonet.agron.iastate.edu/geojson/sbw.py?ts=" + asOf.toString("yyyy-MM-ddTHH:mm:00'Z'").remove('\'').toStdString() : "https://mesonet.agron.iastate.edu/geojson/sbw.geojson";
    const string watchesUrl = "https://mapservices.weather.noaa.gov/eventdriven/rest/services/WWA/watch_warn_adv/MapServer/1/query?where=" +
                              QUrl::toPercentEncoding("sig='A' AND phenom IN ('TO','SV')").toStdString() + "&outFields=*&f=geojson&geometryPrecision=2";
    auto out = std::make_shared<std::vector<Area>>();
    auto problem = std::make_shared<string>();
    host.background(
        [out, problem, warningsUrl, watchesUrl, past] {
            const auto text = UtilityIO::downloadAsByteArray(warningsUrl);
            const auto doc = QJsonDocument::fromJson(text);
            if (!doc.isObject()) {
                *problem = "the warnings could not be read";
            }
            for (const auto& value : doc.object().value("features").toArray()) {
                const auto feature = value.toObject();
                const auto p = feature.value("properties").toObject();
                Area a;
                rings(feature.value("geometry").toObject(), a.rings);
                if (a.rings.empty()) {
                    continue;
                }
                const auto phenomena = p.value("phenomena").toString(), significance = p.value("significance").toString();
                a.kind = significance == "W" ? (phenomena == "TO" ? Tornado : phenomena == "SV" ? Thunderstorm : phenomena == "FF" ? FlashFlood : (phenomena == "FA" || phenomena == "FL") ? Flood : phenomena == "SM" ? Marine : Other)
                                             : (phenomena == "FA" || phenomena == "FL") ? Flood : Other;
                a.strong = p.value("is_emergency").toBool();
                a.begin = secondsOf(p.value("issue").toString());
                a.end = secondsOf(p.value("expire").toString());
                const auto ps = p.value("ps").toString();
                QStringList tags;
                const auto add = [&tags, &p] (const char * key, const QString& label) {
                    const auto v = p.value(key);
                    if (!v.isNull() && !v.toVariant().toString().isEmpty()) {
                        tags << label + " " + v.toVariant().toString();
                    }
                };
                add("tornadotag", "tornado:");
                add("hailtag", "hail:");
                add("windtag", "wind:");
                add("damagetag", "damage threat:");
                add("waterspouttag", "waterspout:");
                add("squalltag", "squall:");
                add("floodtag_damage", "flood damage:");
                QString title = ps;
                if (a.strong) {
                    title += " (EMERGENCY)";
                } else if (p.value("is_pds").toBool()) {
                    title += " (particularly dangerous situation)";
                }
                a.title = title;
                a.text = title + "  -  " + p.value("wfo").toString() + "\nIssued " + zulu(a.begin) + ", expires " + zulu(a.end) + (tags.isEmpty() ? QString{} : "\n" + tags.join(", "));
                const auto product = p.value("product_id").toString();
                if (!product.isEmpty()) {
                    a.textUrl = "https://mesonet.agron.iastate.edu/api/1/nwstext/" + product;
                    a.windowTitle = title + " - " + p.value("wfo").toString();
                }
                out->push_back(std::move(a));
            }
            if (!past) {   // the SPC watches (the NWS offices' parts of each)
                const auto watches = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(watchesUrl));
                for (const auto& value : watches.object().value("features").toArray()) {
                    const auto feature = value.toObject();
                    const auto p = feature.value("properties").toObject();
                    Area a;
                    rings(feature.value("geometry").toObject(), a.rings);
                    if (a.rings.empty()) {
                        continue;
                    }
                    const bool tornado = p.value("phenom").toString() == "TO";
                    a.kind = tornado ? TornadoWatch : StormWatch;
                    a.begin = secondsOf(p.value("issuance").toString());
                    a.end = secondsOf(p.value("ends").toString());
                    const auto number = QString::number(p.value("event").toString().toInt());
                    a.title = p.value("prod_type").toString() + " " + number;
                    a.text = a.title + "  -  part of " + p.value("wfo").toString().mid(1) + "\nIssued " + zulu(a.begin) + ", ends " + zulu(a.end);
                    a.textUrl = "https://www.spc.noaa.gov/products/watch/ww" + QString{"%1"}.arg(number.toInt(), 4, 10, QChar{'0'}) + ".html";
                    a.textUrl.clear();   // (the page is HTML; the watch text is in the NWS's alert, which the url below answers as JSON)
                    const auto alert = p.value("url").toString();
                    if (!alert.isEmpty()) {
                        a.textUrl = alert;
                        a.windowTitle = a.title;
                    }
                    out->push_back(std::move(a));
                }
                // a watch is cut in parts at the offices' borders: one outline for each watch
                std::map<QString, QPainterPath> joined;
                std::map<QString, Area> first;
                std::vector<Area> kept;
                for (auto& a : *out) {
                    if (a.kind != TornadoWatch && a.kind != StormWatch) {
                        kept.push_back(std::move(a));
                        continue;
                    }
                    QPainterPath path;
                    for (const auto& r : a.rings) {
                        QPolygonF polygon;
                        for (const auto& p : r) {
                            polygon << p;
                        }
                        QPainterPath piece;
                        piece.addPolygon(polygon);
                        piece.closeSubpath();
                        path = path.united(piece);
                    }
                    const auto key = a.title;
                    joined[key] = joined.count(key) ? joined[key].united(path) : path;
                    if (!first.count(key)) {
                        first[key] = a;
                    }
                }
                for (auto& [key, a] : first) {
                    a.rings.clear();
                    for (const auto& polygon : joined[key].simplified().toSubpathPolygons()) {
                        std::vector<QPointF> ring(polygon.begin(), polygon.end());
                        if (ring.size() >= 3) {
                            a.rings.push_back(std::move(ring));
                        }
                    }
                    a.text = a.title + "\nIssued " + zulu(a.begin) + ", ends " + zulu(a.end);
                    kept.push_back(std::move(a));
                }
                *out = std::move(kept);
            }
        },
        [this, out, problem, &host, alive = alive, mine] {
            if (!*alive) {
                return;
            }
            loading = false;
            if (mine != generation) {   // the time was changed while it was reading: read again for the new one
                refresh(host);
                return;
            }
            error = *problem;
            if (problem->empty()) {
                areas = std::move(*out);
            }
            host.redraw();
        });
}

void WarningPolygonsLayer::paint(QPainter& painter, MapHost& host) {
    paintAreas(painter, host, areas, [this] (const Area& a) { return wanted(a.kind); }, {Other, Flood, Marine, FlashFlood, TornadoWatch, StormWatch, Thunderstorm, Tornado});
}

MapHit WarningPolygonsLayer::pick(const QPointF& pixels, MapHost& host) const {
    return pickAreas(pixels, host, areas, [this] (const Area& a) { return wanted(a.kind); }, 30);
}

vector<MapLegendRow> WarningPolygonsLayer::legend() const {
    return legendOf({Tornado, Thunderstorm, FlashFlood, Flood, Marine, TornadoWatch, StormWatch}, "Warnings and watches:");
}

QWidget * WarningPolygonsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    for (const int kind : {Tornado, Thunderstorm, FlashFlood, Flood, Marine, Other, TornadoWatch, StormWatch}) {
        auto * box = new QCheckBox{kindNames[kind], widget};
        box->setChecked(wanted(kind));
        QObject::connect(box, &QCheckBox::toggled, [this, kind, changed] (bool on) {
            enabledMask = on ? (enabledMask | (1 << kind)) : (enabledMask & ~(1 << kind));
            Utility::writePref("MM_WARNING_KINDS", std::to_string(enabledMask));
            changed();
        });
        layout->addWidget(box);
    }
    return widget;
}

// ---- the mesoscale discussions ------------------------------------------------------------------------------------------------------------------------------------------------------------------

string MesoscaleDiscussionLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (loading && areas.empty()) {
        return "reading the discussions...";
    }
    return std::to_string(areas.size()) + (areas.size() == 1 ? " discussion in force" : " discussions in force");
}

void MesoscaleDiscussionLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const string url = "https://mapservices.weather.noaa.gov/vector/rest/services/outlooks/spc_mesoscale_discussion/MapServer/0/query?where=1%3D1&outFields=*&f=geojson";
    auto out = std::make_shared<std::vector<Area>>();
    auto problem = std::make_shared<string>();
    host.background(
        [out, problem, url] {
            const auto doc = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(url));
            if (!doc.isObject()) {
                *problem = "the discussions could not be read";
                return;
            }
            for (const auto& value : doc.object().value("features").toArray()) {
                const auto feature = value.toObject();
                const auto p = feature.value("properties").toObject();
                Area a;
                a.kind = Discussion;
                rings(feature.value("geometry").toObject(), a.rings);
                if (a.rings.empty()) {
                    continue;
                }
                const auto name = p.value("name").toString();   // "MD 2402"
                const int number = name.mid(3).trimmed().toInt();
                a.title = name;
                a.text = p.value("folderpath").toString();
                if (number > 0) {
                    const auto file = QString{"%1"}.arg(number, 4, 10, QChar{'0'});
                    const auto full = QString::fromUtf8(UtilityIO::downloadAsByteArray("https://www.spc.noaa.gov/products/md/md" + file.toStdString() + ".txt"));
                    if (!full.isEmpty()) {
                        const auto grab = [&full] (const QString& label) {
                            const QRegularExpression re{label + "\\.\\.\\.([^\\n]*)"};
                            const auto m = re.match(full);
                            return m.hasMatch() ? m.captured(1).trimmed() : QString{};
                        };
                        QString summary;
                        const QRegularExpression sm{"SUMMARY\\.\\.\\.(.*?)\\n\\n", QRegularExpression::DotMatchesEverythingOption};
                        const auto m = sm.match(full);
                        if (m.hasMatch()) {
                            summary = m.captured(1).simplified();
                        }
                        const auto valid = QRegularExpression{"Valid ([0-9]{6}Z - [0-9]{6}Z)"}.match(full);
                        a.text = "Mesoscale Discussion " + QString::number(number) + (valid.hasMatch() ? "  -  valid " + valid.captured(1) : QString{}) + "\n" + grab("Areas affected") + "\nConcerning: " +
                                 grab("Concerning").remove(QRegularExpression{"\\.\\.\\.$"}) + (summary.isEmpty() ? QString{} : "\n" + summary.left(420) + (summary.size() > 420 ? "..." : ""));
                        a.textUrl = "https://www.spc.noaa.gov/products/md/md" + file + ".txt";
                        a.windowTitle = "Mesoscale discussion " + QString::number(number);
                    }
                }
                out->push_back(std::move(a));
            }
        },
        [this, out, problem, &host, alive = alive] {
            if (!*alive) {
                return;
            }
            loading = false;
            error = *problem;
            if (problem->empty()) {
                areas = std::move(*out);
            }
            host.redraw();
        });
}

void MesoscaleDiscussionLayer::paint(QPainter& painter, MapHost& host) {
    paintAreas(painter, host, areas, [] (const Area&) { return true; }, {Discussion});
}

MapHit MesoscaleDiscussionLayer::pick(const QPointF& pixels, MapHost& host) const {
    return pickAreas(pixels, host, areas, [] (const Area&) { return true; }, 28);
}

vector<MapLegendRow> MesoscaleDiscussionLayer::legend() const {
    return legendOf({Discussion}, "SPC:");
}
