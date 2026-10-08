// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "radar/MapView.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QMouseEvent>
#include <QWheelEvent>
#include "radar/Projection.h"
#include "settings/Location.h"

MapView::MapView(Window * parent, int side) {
    widget = new MapWidget{
        parent, 0, 1, true, Location::radarSite(), side, side,
        [this] (double factor, [[maybe_unused]] int pane) { zoomBy(factor); },
        [this] (double dx, double dy, [[maybe_unused]] int pane) { panBy(dx, dy); }};
    widget->setFixedSize(side, side);
    widget->mapState.setRadar(Location::radarSite());
    widget->mapState.reset();
    widget->mapDraw.initGeom();
    widget->setMouseTracking(true);
    widget->installEventFilter(this);
    setParent(widget);
}

double MapView::mercator(double lat) {
    return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
}

MapView::Coefficients MapView::coefficients() const {
    const auto& pn = widget->mapState.getPn();
    const auto project = [&pn] (double lat, double lon) {
        return Projection::computeMercatorNumbersFromLatLon(LatLon{lat, lon}.reverseLon(), pn);
    };
    const auto a = project(30.0, -100.0);
    const auto b = project(30.0, -99.0);
    const auto c = project(40.0, -100.0);
    const double ax = b[0] - a[0];
    const double ay = (c[1] - a[1]) / (mercator(40.0) - mercator(30.0));
    return {ax, a[0] + 100.0 * ax, ay, a[1] - ay * mercator(30.0)};
}

MapView::Transform MapView::transform() const {
    const auto k = coefficients();
    const auto& state = widget->mapState;
    return {k.ax, k.bx, k.ay, k.by, state.zoom, state.xPos, state.yPos};
}

QPointF MapView::toUnits(double lat, double lon) const {
    return transform()(lat, lon);
}

QPointF MapView::toPixels(double lat, double lon) const {
    const auto u = toUnits(lat, lon);
    return QPointF{(u.x() + 500.0) * widget->width() / 1000.0, (u.y() + 250.0) * widget->height() / 1000.0};
}

std::pair<double, double> MapView::toLatLon(const QPointF& pixels) const {
    const auto t = transform();
    const double u = pixels.x() * 1000.0 / std::max(1, widget->width()) - 500.0;
    const double v = pixels.y() * 1000.0 / std::max(1, widget->height()) - 250.0;
    const double lon = ((u - t.xPos) / t.zoom - t.bx) / t.ax;
    const double merc = ((v - t.yPos) / t.zoom - t.by) / t.ay;   // Mercator "degrees"
    const double lat = (2.0 * std::atan(std::exp(merc * std::numbers::pi / 180.0)) - std::numbers::pi / 2.0) * 180.0 / std::numbers::pi;
    return {lat, lon};
}

double MapView::unitsPerPixel() const {
    return 1000.0 / std::max(1, widget->width());
}

bool MapView::inView(const QPointF& units, double margin) const {
    return units.x() > -500.0 - margin && units.x() < 500.0 + margin && units.y() > -250.0 - margin && units.y() < 750.0 + margin;
}

void MapView::showRegion(double minLat, double maxLat, double minLon, double maxLon) {
    auto& state = widget->mapState;
    const auto k = coefficients();
    const double centerLon = (minLon + maxLon) / 2.0;
    const double centerMerc = (mercator(minLat) + mercator(maxLat)) / 2.0;
    const double lonSpan = std::max(1.0, maxLon - minLon);
    const double mercSpan = std::max(1.0, mercator(maxLat) - mercator(minLat));
    state.zoom = std::min(1000.0 / (std::abs(k.ax) * lonSpan), 1000.0 / (std::abs(k.ay) * mercSpan));
    state.xPos = -(k.ax * centerLon + k.bx) * state.zoom;
    state.yPos = 250.0 - (k.ay * centerMerc + k.by) * state.zoom;
    widget->mapTextObject.add();
    widget->update();
}

void MapView::fit(int availableWidth, int availableHeight) {
    const int side = std::max(300, std::min(availableWidth, availableHeight));
    if (widget->width() != side) {
        widget->setFixedSize(side, side);
        widget->mapState.originalWidth = side;
        widget->mapState.originalHeight = side;
        widget->mapTextObject.add();
    }
}

void MapView::zoomBy(double factor) {
    auto& state = widget->mapState;
    if (factor < 1.0 && state.zoom <= 0.02) {
        return;
    }
    const double oldZoom = state.zoom;
    state.zoom = std::min(state.zoom * factor, 60.0);
    const double change = state.zoom / oldZoom;
    double u = 0.0;
    double v = 0.0;
    if (pointerInside) {
        u = pointer.x() * 1000.0 / std::max(1, widget->width()) - 500.0;
        v = pointer.y() * 1000.0 / std::max(1, widget->height()) - 250.0;
    }
    state.xPos = u - (u - state.xPos) * change;
    state.yPos = v - (v - state.yPos) * change;
    widget->mapTextObject.add();
    widget->update();
}

void MapView::panBy(double dx, double dy) {
    auto& state = widget->mapState;
    const double perPixel = unitsPerPixel();
    state.xPos += dx * perPixel;
    state.yPos += dy * perPixel;
    widget->mapTextObject.add();
    widget->update();
}

bool MapView::eventFilter(QObject * object, QEvent * event) {
    if (object != widget) {
        return false;
    }
    switch (event->type()) {
        case QEvent::MouseMove:
            pointer = static_cast<QMouseEvent *>(event)->position();
            pointerInside = true;
            if (onPointer) {
                onPointer(pointer);
            }
            break;
        case QEvent::Wheel:
            pointer = static_cast<QWheelEvent *>(event)->position();
            pointerInside = true;
            break;
        case QEvent::Leave:
            pointerInside = false;
            if (onLeave) {
                onLeave();
            }
            break;
        default:
            break;
    }
    return false;
}
