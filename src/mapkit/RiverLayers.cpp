// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/RiverLayers.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include "buoys/BuoyViewer.h"
#include "dams/DamViewer.h"
#include "rivers/RiverGaugeViewer.h"
#include "util/Utility.h"

namespace {
    struct Look {
        const char * status;
        const char * label;
        QColor color;
        int rank;
        double radius;
    };

    const std::vector<Look>& looks() {
        static const std::vector<Look> table{
            {"out_of_service", "Out of service", QColor{120, 120, 120}, 0, 2.5},
            {"not_defined", "No flood stages defined", QColor{150, 150, 160}, 1, 2.5},
            {"obs_not_current", "Observation not current", QColor{190, 190, 190}, 2, 2.5},
            {"low_threshold", "Below the low-water level", QColor{150, 100, 50}, 3, 3.5},
            {"no_flooding", "No flooding", QColor{40, 190, 70}, 4, 3.0},
            {"action", "Action stage", QColor{240, 210, 0}, 5, 4.5},
            {"minor", "Minor flooding", QColor{255, 140, 0}, 6, 5.5},
            {"moderate", "Moderate flooding", QColor{230, 40, 40}, 7, 6.5},
            {"major", "Major flooding", QColor{170, 40, 240}, 8, 7.5},
        };
        return table;
    }

    const Look& lookOf(const string& status) {
        for (const auto& look : looks()) {
            if (status == look.status) {
                return look;
            }
        }
        return looks()[1];
    }

    // pixels from window units
    QPointF pixelsOf(const QPointF& units, double perPixel) {
        return QPointF{(units.x() + 500.0) / perPixel, (units.y() + 250.0) / perPixel};
    }

    bool onScreen(const QPointF& u) {
        return u.x() >= -520.0 && u.x() <= 520.0 && u.y() >= -270.0 && u.y() <= 770.0;
    }
}

// ---- river gauges ----

string GaugeLayer::summary() const {
    if (loading && !gauges) return "reading the river gauges...";
    if (!error.empty()) return error;
    if (!gauges) return {};
    std::map<string, int> counts;
    for (const auto& g : *gauges) counts[g.status]++;
    return std::to_string(gauges->size()) + " gauges (" + std::to_string(counts["major"] + counts["moderate"] + counts["minor"]) + " in flood, " + std::to_string(counts["action"]) + " at action stage)";
}

void GaugeLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<UtilityRivers::Gauge>>();
    auto message = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    host.background(
        [fresh, message, ok] {
            *ok = UtilityRivers::loadGauges(*fresh, *message);
            std::stable_sort(fresh->begin(), fresh->end(), [] (const auto& a, const auto& b) { return lookOf(a.status).rank < lookOf(b.status).rank; });   // the worst on top
        },
        [this, &host, fresh, message, ok] {
            loading = false;
            if (*ok) {
                gauges = fresh;
                error.clear();
            } else {
                error = *message;
            }
            host.redraw();
        });
}

bool GaugeLayer::shown(const UtilityRivers::Gauge& g) const {
    const auto& s = g.status;
    switch (filter) {
        case 1: return s == "action" || s == "minor" || s == "moderate" || s == "major";
        case 2: return s == "minor" || s == "moderate" || s == "major";
        case 3: return s == "obs_not_current" || s == "out_of_service";
        default: return true;
    }
}

void GaugeLayer::paint(QPainter& painter, MapHost& host) {
    if (!gauges) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double perPixel = view.unitsPerPixel();
    for (const auto& g : *gauges) {
        if (!shown(g)) {
            continue;
        }
        const QPointF u = t(g.lat, g.lon);
        if (!onScreen(u)) {
            continue;
        }
        const auto& look = lookOf(g.status);
        const double r = (look.radius + std::min(3.0, std::log2(std::max(1.0, t.zoom * 7.0)) * 0.5)) * perPixel;
        painter.setPen(QPen{QColor{0, 0, 0, 170}, 0.9 * perPixel});
        painter.setBrush(look.color);
        painter.drawEllipse(u, r, r);
    }
}

MapHit GaugeLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 11.0;
    hit.priority = 5;
    if (!gauges) {
        return hit;
    }
    auto& view = host.view();
    for (const auto& g : *gauges) {
        if (!shown(g)) {
            continue;
        }
        const auto at = view.toPixels(g.lat, g.lon);
        if (std::abs(at.x() - pixels.x()) > hit.reach || std::abs(at.y() - pixels.y()) > hit.reach) {
            continue;
        }
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance <= hit.distance) {   // the worse gauge wins a tie (they are sorted worst last)
            hit.distance = distance;
            QString text = QString::fromStdString(g.lid + "  " + (g.name.empty() ? g.waterbody : g.name)) + "\n" + QString::fromStdString(g.waterbody + ", " + g.state) + "\n" +
                lookOf(g.status).label;
            if (UtilityRivers::has(g.observed)) {
                text += "   " + QString::number(g.observed, 'f', 2) + " " + QString::fromStdString(g.units);
            }
            if (!g.obsTime.empty()) {
                text += "\n" + QString::fromStdString(g.obsTime) + " UTC";
            }
            hit.text = text;
            const string lid = g.lid;
            hit.open = [lid] (Window * parent) { new RiverGaugeViewer{parent, lid}; };
        }
    }
    return hit;
}

vector<MapLegendRow> GaugeLayer::legend() const {
    MapLegendRow row;
    row.title = "River gauges:";
    for (const auto& [status, name] : {std::pair<const char *, const char *>{"major", "Major"}, {"moderate", "Moderate"}, {"minor", "Minor"}, {"action", "Action"}, {"no_flooding", "No flooding"},
                                       {"low_threshold", "Low water"}, {"obs_not_current", "Not current"}}) {
        row.entries.push_back({MapLegendEntry::Circle, lookOf(status).color, name});
    }
    return {row};
}

QWidget * GaugeLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Show:", widget});
    auto * combo = new QComboBox{widget};
    combo->addItems({"All gauges", "Action stage or higher", "Flooding (minor or higher)", "Not reporting (no current reading / out of service)"});
    combo->setCurrentIndex(filter);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) { filter = index; changed(); });
    layout->addWidget(combo);
    return widget;
}

// ---- dams ----

string DamLayer::summary() const {
    return loading && !dams ? "reading the dams..." : dams ? std::to_string(dams->size()) + " Corps dams" : string{};
}

void DamLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<DamData::Latest>>();
    host.background([fresh] { DamData::loadLatest(*fresh); }, [this, &host, fresh] {
        loading = false;
        dams = fresh;
        host.redraw();
    });
}

void DamLayer::paint(QPainter& painter, MapHost& host) {
    if (!dams) {
        return;
    }
    const auto t = host.view().transform();
    const double perPixel = host.view().unitsPerPixel();
    for (const auto& d : *dams) {
        if (d.project == nullptr) {
            continue;
        }
        const QPointF u = t(d.project->lat, d.project->lon);
        if (!onScreen(u)) {
            continue;
        }
        const bool generating = UtilityDams::has(d.generation) && d.generation > 0.0;
        const QColor color = !d.ok ? QColor{130, 130, 140} : generating ? QColor{255, 150, 20} : QColor{50, 130, 235};
        const double release = UtilityDams::has(d.outflow) ? d.outflow : 0.0;
        const double radius = (7.0 + std::min(5.0, std::log10(1.0 + release) * 1.2) + std::min(2.0, std::log2(std::max(1.0, t.zoom * 7.0)) * 0.3)) * perPixel;
        painter.setPen(QPen{QColor{255, 255, 255}, 1.4 * perPixel});
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF{{QPointF{u.x(), u.y() - radius}, QPointF{u.x() + radius, u.y()}, QPointF{u.x(), u.y() + radius}, QPointF{u.x() - radius, u.y()}}});
    }
}

MapHit DamLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 15.0;
    hit.priority = 15;   // a dam over the gauge beside it
    if (!dams) {
        return hit;
    }
    for (const auto& d : *dams) {
        if (d.project == nullptr) {
            continue;
        }
        const auto at = host.view().toPixels(d.project->lat, d.project->lon);
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = QString::fromStdString(d.project->name + "  (Corps of Engineers)") + "\n" + DamViewer::summary(d).replace(";  ", "\n");
            const auto * project = d.project;
            hit.open = [project] (Window * parent) { new DamViewer{parent, *project}; };
        }
    }
    return hit;
}

vector<MapLegendRow> DamLayer::legend() const {
    return {MapLegendRow{"Dams:", {{MapLegendEntry::Diamond, QColor{255, 150, 20}, "generating"}, {MapLegendEntry::Diamond, QColor{50, 130, 235}, "releasing"},
                                   {MapLegendEntry::Diamond, QColor{130, 130, 140}, "no recent data"}}}};
}

// ---- buoys ----

string BuoyLayer::summary() const {
    return loading && !buoys ? "reading the buoys..." : buoys ? std::to_string(buoys->size()) + " buoys and coastal stations" : string{};
}

void BuoyLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<vector<BuoyData::Marker>>();
    auto message = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    host.background([fresh, message, ok] { *ok = BuoyData::loadLatest(*fresh, *message); }, [this, &host, fresh, ok] {
        loading = false;
        if (*ok) {
            buoys = fresh;
        }
        host.redraw();
    });
}

QColor BuoyLayer::colorOf(const BuoyData::Marker& m) const {
    if (colorBy == 1) {
        if (!UtilityBuoys::has(m.obs.waterTemperature)) {
            return QColor{120, 120, 130};
        }
        const double f = UtilityBuoys::fahrenheit(m.obs.waterTemperature);
        return f < 40 ? QColor{150, 90, 220} : f < 50 ? QColor{60, 100, 230} : f < 60 ? QColor{40, 190, 220} : f < 70 ? QColor{60, 190, 90} : f < 80 ? QColor{240, 200, 40} : QColor{235, 70, 50};
    }
    if (!UtilityBuoys::has(m.obs.windSpeed)) {
        return QColor{120, 120, 130};
    }
    const double kt = UtilityBuoys::knots(m.obs.windSpeed);
    return kt < 10 ? QColor{130, 170, 205} : kt < 20 ? QColor{60, 190, 90} : kt < 34 ? QColor{240, 210, 40} : kt < 48 ? QColor{255, 140, 0} : kt < 64 ? QColor{230, 40, 40} : QColor{170, 40, 240};
}

void BuoyLayer::paint(QPainter& painter, MapHost& host) {
    if (!buoys) {
        return;
    }
    const auto t = host.view().transform();
    const double perPixel = host.view().unitsPerPixel();
    const double half = (4.2 + std::min(2.5, std::log2(std::max(1.0, t.zoom * 7.0)) * 0.4)) * perPixel;
    for (const auto& m : *buoys) {
        const QPointF u = t(m.obs.lat, m.obs.lon);
        if (!onScreen(u)) {
            continue;
        }
        painter.setPen(QPen{QColor{255, 255, 255, 200}, 1.0 * perPixel});
        painter.setBrush(colorOf(m));
        painter.drawRect(QRectF{u.x() - half, u.y() - half, 2 * half, 2 * half});
    }
}

MapHit BuoyLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 11.0;
    hit.priority = 8;
    if (!buoys) {
        return hit;
    }
    for (const auto& m : *buoys) {
        const auto at = host.view().toPixels(m.obs.lat, m.obs.lon);
        if (std::abs(at.x() - pixels.x()) > hit.reach || std::abs(at.y() - pixels.y()) > hit.reach) {
            continue;
        }
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = QString::fromStdString(m.obs.id + (m.station.name.empty() ? "" : "  " + m.station.name)) + "\n" + BuoyViewer::summary(m).replace(";  ", "\n");
            const auto copy = m;
            hit.open = [copy] (Window * parent) { new BuoyViewer{parent, copy}; };
        }
    }
    return hit;
}

vector<MapLegendRow> BuoyLayer::legend() const {
    MapLegendRow row;
    row.title = colorBy == 1 ? "Buoys, water:" : "Buoys, wind:";
    if (colorBy == 1) {
        row.entries = {{MapLegendEntry::Square, QColor{150, 90, 220}, "<40 F"}, {MapLegendEntry::Square, QColor{60, 100, 230}, "40-50"}, {MapLegendEntry::Square, QColor{40, 190, 220}, "50-60"},
                       {MapLegendEntry::Square, QColor{60, 190, 90}, "60-70"}, {MapLegendEntry::Square, QColor{240, 200, 40}, "70-80"}, {MapLegendEntry::Square, QColor{235, 70, 50}, "80+ F"}};
    } else {
        row.entries = {{MapLegendEntry::Square, QColor{130, 170, 205}, "<10 kt"}, {MapLegendEntry::Square, QColor{60, 190, 90}, "10-20"}, {MapLegendEntry::Square, QColor{240, 210, 40}, "20-34"},
                       {MapLegendEntry::Square, QColor{255, 140, 0}, "34-48"}, {MapLegendEntry::Square, QColor{230, 40, 40}, "48-64"}, {MapLegendEntry::Square, QColor{170, 40, 240}, "64+ kt"}};
    }
    return {row};
}

QWidget * BuoyLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Colour by:", widget});
    auto * combo = new QComboBox{widget};
    combo->addItems({"Wind", "Water temperature"});
    combo->setCurrentIndex(colorBy);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) { colorBy = index; changed(); });
    layout->addWidget(combo);
    return widget;
}
