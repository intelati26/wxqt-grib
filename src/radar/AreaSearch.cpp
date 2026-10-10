// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "radar/AreaSearch.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>

namespace {
    double kilometers(double lat1, double lon1, double lat2, double lon2) {
        const double rad = std::numbers::pi / 180.0;
        const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) + std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
        return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
    }
}

AreaSearch::AreaSearch(MapView * mapView, Button * areaButton, std::function<double()> radiusKm, std::function<void()> onChanged)
    : QObject{mapView}
    , view{mapView}
    , button{areaButton}
    , radius{std::move(radiusKm)}
    , changed{std::move(onChanged)}
{
    button->setText("Search an area");
    button->getView()->setToolTip("Drag a box on the map, or click a point for a circle (the radius is set by the 'within' box), to find what passed through it");
    button->connect([this] { toggle(); });
    view->map()->installEventFilter(this);
}

void AreaSearch::clear() {
    kind = None;
    mode = false;
    dragging = false;
    button->setText("Search an area");
    view->map()->setCursor(Qt::ArrowCursor);
}

void AreaSearch::toggle() {
    if (kind != None) {
        clear();   // "Clear area"
        changed();
        return;
    }
    mode = !mode;
    button->setText(mode ? "Drag a box or click a point (click here to cancel)" : "Search an area");
    view->map()->setCursor(mode ? Qt::CrossCursor : Qt::ArrowCursor);
}

bool AreaSearch::eventFilter(QObject * object, QEvent * event) {
    if (object != view->map() || !mode) {
        return false;
    }
    switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto * e = static_cast<QMouseEvent *>(event);
            if (e->button() != Qt::LeftButton) {
                return false;
            }
            dragging = true;
            dragStart = dragNow = e->position();
            return true;
        }
        case QEvent::MouseMove:
            if (dragging) {
                dragNow = static_cast<QMouseEvent *>(event)->position();
                view->map()->update();
                return true;
            }
            return false;
        case QEvent::MouseButtonRelease: {
            if (!dragging) {
                return false;
            }
            dragging = false;
            const QPointF end = static_cast<QMouseEvent *>(event)->position();
            mode = false;
            view->map()->setCursor(Qt::ArrowCursor);
            if (std::hypot(end.x() - dragStart.x(), end.y() - dragStart.y()) < 6.0) {   // a click: a circle round the point
                const auto [clickLat, clickLon] = view->toLatLon(end);
                kind = Circle;
                lat = clickLat;
                lon = clickLon;
                radiusKm = radius();
            } else {
                const auto a = view->toLatLon(dragStart);
                const auto b = view->toLatLon(end);
                kind = Box;
                minLat = std::min(a.first, b.first);
                maxLat = std::max(a.first, b.first);
                minLon = std::min(a.second, b.second);
                maxLon = std::max(a.second, b.second);
            }
            button->setText("Clear area");
            changed();
            view->map()->update();
            return true;
        }
        default:
            return false;
    }
}

bool AreaSearch::hitsPoint(double pointLat, double pointLon) const {
    if (kind == Box) {
        return pointLat >= minLat && pointLat <= maxLat && pointLon >= minLon && pointLon <= maxLon;
    }
    if (kind == Circle) {
        return std::abs(pointLat - lat) < radiusKm / 111.0 + 1.0 && kilometers(lat, lon, pointLat, pointLon) <= radiusKm;
    }
    return true;
}

bool AreaSearch::hitsSegment(double lat1, double lon1, double lat2, double lon2) const {
    if (kind == None) {
        return true;
    }
    if (hitsPoint(lat1, lon1) || hitsPoint(lat2, lon2)) {
        return true;
    }
    if (std::abs(lon1 - lon2) > 100.0) {
        return false;   // across the date line
    }
    if (kind == Circle) {
        return hitsPoint((lat1 + lat2) / 2.0, (lon1 + lon2) / 2.0);
    }
    // Liang-Barsky: does the piece cross the box
    double t0 = 0.0;
    double t1 = 1.0;
    const double dx = lon2 - lon1;
    const double dy = lat2 - lat1;
    const double pr[4] = {-dx, dx, -dy, dy};
    const double qr[4] = {lon1 - minLon, maxLon - lon1, lat1 - minLat, maxLat - lat1};
    for (int e = 0; e < 4; e++) {
        if (pr[e] == 0.0) {
            if (qr[e] < 0.0) {
                return false;
            }
        } else {
            const double r = qr[e] / pr[e];
            if (pr[e] < 0.0) {
                t0 = std::max(t0, r);
            } else {
                t1 = std::min(t1, r);
            }
            if (t0 > t1) {
                return false;
            }
        }
    }
    return true;
}

void AreaSearch::paint(QPainter& painter, const MapView::Transform& t, double px) const {
    painter.setBrush(QColor{255, 255, 255, 25});
    painter.setPen(QPen{QColor{255, 255, 255, 230}, 1.5 * px, Qt::DashLine});
    if (kind == Box) {
        painter.drawPolygon(QPolygonF{{t(maxLat, minLon), t(maxLat, maxLon), t(minLat, maxLon), t(minLat, minLon)}});
    } else if (kind == Circle) {
        QPolygonF ring;
        for (int step = 0; step < 72; step++) {
            const double bearing = step * 5.0 * std::numbers::pi / 180.0;
            ring << t(lat + radiusKm / 111.0 * std::cos(bearing), lon + radiusKm / (111.0 * std::max(0.2, std::cos(lat * std::numbers::pi / 180.0))) * std::sin(bearing));
        }
        painter.drawPolygon(ring);
    }
    if (dragging) {
        const auto toUnits = [this] (const QPointF& pixels) {
            return QPointF{pixels.x() * 1000.0 / std::max(1, view->map()->width()) - 500.0, pixels.y() * 1000.0 / std::max(1, view->map()->height()) - 250.0};
        };
        painter.drawRect(QRectF{toUnits(dragStart), toUnits(dragNow)}.normalized());
    }
}

QString AreaSearch::describe() const {
    if (kind == Box) {
        return "in the box " + QString::number(minLat, 'f', 1) + " to " + QString::number(maxLat, 'f', 1) + " N, " + QString::number(minLon, 'f', 1) + " to " + QString::number(maxLon, 'f', 1) + " E";
    }
    if (kind == Circle) {
        return "within " + QString::number(static_cast<int>(radiusKm)) + " km of " + QString::number(lat, 'f', 1) + " N, " + QString::number(lon, 'f', 1) + " E";
    }
    return {};
}
