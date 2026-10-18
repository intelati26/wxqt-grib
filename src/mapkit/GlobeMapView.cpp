// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GlobeMapView.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>

namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double rad = pi / 180.0;
}

GlobeMapView::GlobeMapView(QWidget * parent) : QWidget{parent} {
    setMouseTracking(true);
    setMinimumSize(420, 420);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, this, [this] { makeNow(); });
}

double GlobeMapView::radius() const {
    return std::min(width(), height()) / 2.0 * 0.97 * zoom;
}

QPointF GlobeMapView::center() const {
    return QPointF{width() / 2.0, height() / 2.0};
}

void GlobeMapView::scheduleSkin(int milliseconds) {
    timer.start(milliseconds);
}

bool GlobeMapView::place(const QPointF& p, double& lat, double& lon) const {
    const double R = radius();
    const double x = (p.x() - center().x()) / R, y = (center().y() - p.y()) / R;
    const double rho = std::hypot(x, y);
    if (rho > 1.0) {
        return false;
    }
    if (rho < 1e-9) {
        lat = lat0;
        lon = lon0;
        return true;
    }
    const double c = std::asin(rho), phi0 = lat0 * rad;
    lat = std::asin(std::cos(c) * std::sin(phi0) + y * std::sin(c) * std::cos(phi0) / rho) / rad;
    lon = lon0 + std::atan2(x * std::sin(c), rho * std::cos(phi0) * std::cos(c) - y * std::sin(phi0) * std::sin(c)) / rad;
    while (lon > 180.0) {
        lon -= 360.0;
    }
    while (lon < -180.0) {
        lon += 360.0;
    }
    return true;
}

// the skin for the part of the world in view: the whole world while it is all there, the cap of the globe that shows when it is zoomed
void GlobeMapView::makeNow() {
    if (!makeSkin || width() < 100) {
        return;
    }
    double south = -85.0, north = 85.0, west = lon0 - 180.0, east = lon0 + 180.0;
    int pixels = 3072;
    if (zoom >= 1.5) {
        const double theta = std::asin(std::min(1.0, 1.0 / zoom)) / rad;   // the angle from the middle to the rim of what shows
        const double half = std::min(90.0, theta * 1.3 + 2.0);
        south = std::max(-85.0, lat0 - half);
        north = std::min(85.0, lat0 + half);
        const double lonHalf = std::min(180.0, half / std::max(0.15, std::cos(std::min(85.0, std::abs(lat0)) * rad)));
        west = lon0 - lonHalf;
        east = lon0 + lonHalf;
        pixels = std::clamp(static_cast<int>(std::max(width(), height()) * 2.0), 1024, 4096);
    }
    skin = makeSkin(south, north, west, east, pixels);
    update();
}

void GlobeMapView::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.fillRect(rect(), QColor{6, 8, 14});
    if (!skin.valid()) {
        p.setPen(QColor{220, 225, 235});
        p.drawText(rect(), Qt::AlignCenter, "drawing the layers on the globe ...");
        if (!timer.isActive()) {
            scheduleSkin(50);
        }
        return;
    }
    const double scale = dragging ? 0.5 : 1.0;
    const int w = static_cast<int>(width() * scale), h = static_cast<int>(height() * scale);
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const double R = radius() * scale, cx = center().x() * scale, cy = center().y() * scale, phi0 = lat0 * rad;
    const double sinP0 = std::sin(phi0), cosP0 = std::cos(phi0);
    const int top = std::max(0, static_cast<int>(cy - R) - 1), bottom = std::min(h - 1, static_cast<int>(cy + R) + 1);
    const int left = std::max(0, static_cast<int>(cx - R) - 1), right = std::min(w - 1, static_cast<int>(cx + R) + 1);
    const auto t = skin.transform;
    const double skinCentre = (skin.west + skin.east) / 2.0;
    const double toPixels = skin.width / 1000.0;
    const QRgb background = skin.image.pixel(0, 0) | 0xff000000u;   // the map's own background, where the skin has nothing (the poles)
    for (int py = top; py <= bottom; py++) {
        auto * row = reinterpret_cast<QRgb *>(image.scanLine(py));
        const double y = (cy - py) / R;
        for (int px = left; px <= right; px++) {
            const double x = (px - cx) / R;
            const double rho = std::hypot(x, y);
            if (rho > 1.0) {
                continue;
            }
            const double c = std::asin(rho);
            double lat = lat0, lon = lon0;
            if (rho > 1e-9) {
                lat = std::asin(std::cos(c) * sinP0 + y * std::sin(c) * cosP0 / rho) / rad;
                lon = lon0 + std::atan2(x * std::sin(c), rho * cosP0 * std::cos(c) - y * sinP0 * std::sin(c)) / rad;
            }
            QRgb color = background;
            if (lat > -85.0 && lat < 85.0) {
                // the longitude as the skin has it (within half a turn of its middle)
                double lonSkin = lon;
                while (lonSkin - skinCentre > 180.0) {
                    lonSkin -= 360.0;
                }
                while (lonSkin - skinCentre < -180.0) {
                    lonSkin += 360.0;
                }
                const auto units = t(lat, lonSkin);
                const int sx = static_cast<int>((units.x() + 500.0) * toPixels), sy = static_cast<int>((units.y() + 250.0) * toPixels);
                if (sx >= 0 && sy >= 0 && sx < skin.width && sy < skin.height) {
                    const QRgb s = reinterpret_cast<const QRgb *>(skin.image.constScanLine(sy))[sx];
                    if (qAlpha(s) > 0) {
                        color = s;
                    }
                }
            }
            const double shade = 0.62 + 0.38 * std::cos(c);   // a little darker toward the rim, so that it reads as a ball
            row[px] = qRgb(static_cast<int>(qRed(color) * shade), static_cast<int>(qGreen(color) * shade), static_cast<int>(qBlue(color) * shade));
        }
    }
    p.drawImage(rect(), image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen{QColor{200, 210, 230, 150}, 1.5});
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center(), radius(), radius());
    if (paintLegend && !dragging) {
        paintLegend(p, width(), height());
    }
}

void GlobeMapView::mousePressEvent(QMouseEvent * e) {
    last = e->position();
    dragging = true;
    moved = false;
}

void GlobeMapView::mouseReleaseEvent(QMouseEvent * e) {
    const bool wasDrag = moved;
    dragging = false;
    if (wasDrag) {
        scheduleSkin(80);
    } else if (hover.valid() && hover.open) {   // a click on a mark: what the layer opens
        hover.open(dynamic_cast<Window *>(window()));
    }
    update();
    (void)e;
}

void GlobeMapView::mouseMoveEvent(QMouseEvent * e) {
    if (dragging && (e->buttons() & Qt::LeftButton)) {
        if ((e->position() - last).manhattanLength() > 2) {
            moved = true;
        }
        const double perPixel = 1.0 / radius() / rad;
        lon0 -= (e->position().x() - last.x()) * perPixel / std::max(0.2, std::cos(lat0 * rad));
        lat0 = std::clamp(lat0 + (e->position().y() - last.y()) * perPixel, -90.0, 90.0);
        while (lon0 > 180.0) {
            lon0 -= 360.0;
        }
        while (lon0 < -180.0) {
            lon0 += 360.0;
        }
        last = e->position();
        update();
        return;
    }
    double lat, lon;
    hover = {};
    if (skin.valid() && pick && place(e->position(), lat, lon)) {
        hover = pick(skin, lat, lon);
    }
    if (hover.valid()) {
        QToolTip::showText(e->globalPosition().toPoint(), hover.text, this);
        setCursor(hover.open ? Qt::PointingHandCursor : Qt::ArrowCursor);
    } else {
        QToolTip::hideText();
        setCursor(Qt::ArrowCursor);
    }
}

void GlobeMapView::wheelEvent(QWheelEvent * e) {
    zoom = std::clamp(zoom * std::pow(1.0015, e->angleDelta().y()), 0.6, 20.0);
    update();
    scheduleSkin(300);
}
