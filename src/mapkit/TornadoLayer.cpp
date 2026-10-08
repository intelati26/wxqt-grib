// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/TornadoLayer.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QLabel>
#include <QPainterPath>
#include <QVBoxLayout>

namespace {
    QColor colorOf(int mag) {
        switch (mag) {
            case 0: return QColor{110, 220, 255};
            case 1: return QColor{80, 220, 90};
            case 2: return QColor{255, 235, 60};
            case 3: return QColor{255, 150, 30};
            case 4: return QColor{250, 60, 60};
            case 5: return QColor{255, 70, 230};
            default: return QColor{150, 150, 160};
        }
    }

    QString text(const UtilityTornado::Tornado& t) {
        return QString::fromStdString(UtilityTornado::isoDate(t.year, t.month, t.day)) + "  " + QString::fromStdString(t.state) + "  " + QString::fromStdString(UtilityTornado::rating(t)) +
            "\n" + QString::number(t.length, 'f', 1) + " miles, " + QString::number(static_cast<int>(t.width)) + " yards\n" + QString::number(t.fatalities) + " fatalities, " + QString::number(t.injuries) + " injuries";
    }
}

string TornadoLayer::summary() const {
    if (loading && !db) return "reading the tornado database...";
    if (!db) return {};
    if (!db->error.empty()) return db->error;
    size_t n = 0;
    for (const auto& t : db->tornadoes) n += wanted(t) ? 1 : 0;
    return std::to_string(n) + " tornadoes (" + std::to_string(db->lastYear) + " and before)";
}

void TornadoLayer::onEnable(MapHost& host) {
    if (db || loading) {
        return;
    }
    loading = true;
    auto loaded = std::make_shared<std::shared_ptr<const TornadoData::Database>>();
    host.background([loaded] { *loaded = TornadoData::load(); }, [this, &host, loaded] {
        loading = false;
        db = *loaded;
        host.redraw();
    });
}

bool TornadoLayer::wanted(const UtilityTornado::Tornado& t) const {
    if (!t.counts()) {
        return false;
    }
    static const int spans[] = {1, 5, 10, 100000};
    if (t.year <= db->lastYear - spans[std::clamp(years, 0, 3)]) {
        return false;
    }
    return UtilityTornado::passesRating(t, rating);
}

void TornadoLayer::paint(QPainter& painter, MapHost& host) {
    if (!db || !db->error.empty()) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QPainterPath lines[7];
    QPolygonF dots[7];
    const auto inView = [] (const QPointF& p) { return p.x() > -540.0 && p.x() < 540.0 && p.y() > -290.0 && p.y() < 790.0; };
    size_t count = 0;
    for (const auto& tornado : db->tornadoes) {
        if (!wanted(tornado)) {
            continue;
        }
        const auto a = t(tornado.startLat, tornado.startLon);
        const int rank = tornado.mag < 0 ? 0 : tornado.mag + 1;
        if (tornado.hasEnd()) {
            const auto b = t(tornado.endLat, tornado.endLon);
            if (!inView(a) && !inView(b)) {
                continue;
            }
            lines[rank].moveTo(a);
            lines[rank].lineTo(b);
        } else if (!inView(a)) {
            continue;
        }
        dots[rank] << a;
        count++;
    }
    const double scale = count > 5000 ? 0.75 : 1.0;
    for (int rank = 0; rank < 7; rank++) {
        const QColor color = colorOf(rank - 1);
        const double width = (1.3 + 0.45 * (rank - 1 < 0 ? 0 : rank - 1)) * scale;
        painter.setPen(QPen{color, width * px, Qt::SolidLine, Qt::RoundCap});
        painter.drawPath(lines[rank]);
        painter.setPen(QPen{color, (width + 2.0) * px, Qt::SolidLine, Qt::RoundCap});
        painter.drawPoints(dots[rank]);
    }
}

MapHit TornadoLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 10.0;
    hit.priority = 12;
    if (!db || !db->error.empty()) {
        return hit;
    }
    auto& view = host.view();
    for (const auto& tornado : db->tornadoes) {
        if (!wanted(tornado)) {
            continue;
        }
        const auto a = view.toPixels(tornado.startLat, tornado.startLon);
        if (std::abs(a.x() - pixels.x()) > 400 || std::abs(a.y() - pixels.y()) > 400) {   // far from the pointer on the screen (a track is a few tens of kilometres)
            continue;
        }
        double d = std::hypot(a.x() - pixels.x(), a.y() - pixels.y());
        if (tornado.hasEnd()) {
            const auto b = view.toPixels(tornado.endLat, tornado.endLon);
            const QPointF delta = b - a;
            const double length2 = delta.x() * delta.x() + delta.y() * delta.y();
            const double f = length2 > 0.0 ? std::clamp(((pixels.x() - a.x()) * delta.x() + (pixels.y() - a.y()) * delta.y()) / length2, 0.0, 1.0) : 0.0;
            const QPointF nearest = a + delta * f;
            d = std::min(d, std::hypot(nearest.x() - pixels.x(), nearest.y() - pixels.y()));
        }
        if (d < hit.distance) {
            hit.distance = d;
            hit.text = text(tornado);
        }
    }
    return hit;
}

vector<MapLegendRow> TornadoLayer::legend() const {
    MapLegendRow row;
    row.title = "Tornadoes:";
    static const char * names[] = {"F/EF0", "1", "2", "3", "4", "5", "unrated"};
    for (int m = 0; m < 6; m++) {
        row.entries.push_back({MapLegendEntry::Circle, colorOf(m), names[m]});
    }
    row.entries.push_back({MapLegendEntry::Circle, colorOf(-1), names[6]});
    return {row};
}

QWidget * TornadoLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Years:", widget});
    auto * span = new QComboBox{widget};
    span->addItems({"The last year of the data", "The last 5 years", "The last 10 years", "All years since 1950"});
    span->setCurrentIndex(years);
    layout->addWidget(span);
    layout->addWidget(new QLabel{"Rating:", widget});
    auto * level = new QComboBox{widget};
    level->addItems({"All tornadoes", "EF1 or stronger", "EF2 or stronger", "EF3 or stronger"});
    level->setCurrentIndex(rating);
    layout->addWidget(level);
    QObject::connect(span, &QComboBox::currentIndexChanged, [this, changed] (int index) { years = index; changed(); });
    QObject::connect(level, &QComboBox::currentIndexChanged, [this, changed] (int index) { rating = index; changed(); });
    return widget;
}
