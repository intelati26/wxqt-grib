// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GlobeMapView.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>

namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double rad = pi / 180.0;
}

namespace {
    // Equal Earth (Savric, Patterson and Jenny 2018)
    constexpr double A1 = 1.340264, A2 = -0.081106, A3 = 0.000893, A4 = 0.003796;
    const double M = std::sqrt(3.0) / 2.0;
}

const char * GlobeMapView::projectionName(Projection p) {
    return p == Projection::Globe ? "Globe" : p == Projection::Robinson ? "Robinson (as the United Nations' world maps)" : p == Projection::EqualEarth ? "Equal Earth (equal area)" : "Mollweide (equal area)";
}

namespace {
    // Robinson's table, every 5 degrees of latitude from the equator: the length of the parallel and the distance from the equator, as shares of their largest
    constexpr double robinsonLength[19] = {1.0000, 0.9986, 0.9954, 0.9900, 0.9822, 0.9730, 0.9600, 0.9427, 0.9216, 0.8962, 0.8679, 0.8350, 0.7986, 0.7597, 0.7186, 0.6732, 0.6213, 0.5722, 0.5322};
    constexpr double robinsonHeight[19] = {0.0000, 0.0620, 0.1240, 0.1860, 0.2480, 0.3100, 0.3720, 0.4340, 0.4958, 0.5571, 0.6176, 0.6769, 0.7346, 0.7903, 0.8435, 0.8936, 0.9394, 0.9761, 1.0000};
    constexpr double robinsonX = 0.8487, robinsonY = 1.3523;

    // a table value at a latitude (degrees, either hemisphere), interpolated
    double robinsonAt(const double * table, double lat) {
        const double a = std::min(90.0, std::abs(lat)) / 5.0;
        const int i = std::min(17, static_cast<int>(a));
        return table[i] + (table[i + 1] - table[i]) * (a - i);
    }
}

void GlobeMapView::extent(Projection p, double& halfWidth, double& halfHeight) {
    if (p == Projection::Robinson) {
        halfWidth = robinsonX * pi;
        halfHeight = robinsonY;
    } else if (p == Projection::Mollweide) {
        halfWidth = 2.0 * std::sqrt(2.0);
        halfHeight = std::sqrt(2.0);
    } else {
        halfWidth = 2.0 * std::sqrt(3.0) * pi / (3.0 * A1);   // at the equator: x = 2 sqrt(3) lambda / (3 A1)
        const double theta = pi / 3.0;   // the pole: sin(theta) = sqrt(3)/2 * 1
        halfHeight = theta * (A1 + A2 * theta * theta + A3 * std::pow(theta, 6) + A4 * std::pow(theta, 8));
    }
}

bool GlobeMapView::forward(Projection p, double lat, double lon, double& x, double& y) {
    const double phi = lat * rad, lambda = lon * rad;
    if (p == Projection::Robinson) {
        x = robinsonX * robinsonAt(robinsonLength, lat) * lambda;
        y = robinsonY * robinsonAt(robinsonHeight, lat) * (lat < 0 ? -1.0 : 1.0);
        return true;
    }
    if (p == Projection::Mollweide) {
        double theta = phi;
        for (int i = 0; i < 20 && std::abs(std::abs(phi) - pi / 2.0) > 1e-9; i++) {
            const double f = 2.0 * theta + std::sin(2.0 * theta) - pi * std::sin(phi);
            const double d = 2.0 + 2.0 * std::cos(2.0 * theta);
            if (std::abs(d) < 1e-12) {
                break;
            }
            theta -= f / d;
        }
        x = 2.0 * std::sqrt(2.0) / pi * lambda * std::cos(theta);
        y = std::sqrt(2.0) * std::sin(theta);
        return true;
    }
    const double theta = std::asin(M * std::sin(phi));
    const double t2 = theta * theta, t6 = t2 * t2 * t2;
    x = 2.0 * std::sqrt(3.0) * lambda * std::cos(theta) / (3.0 * (9.0 * A4 * t6 * t2 + 7.0 * A3 * t6 + 3.0 * A2 * t2 + A1));
    y = theta * (A1 + A2 * t2 + A3 * t6 + A4 * t6 * t2);
    return true;
}

bool GlobeMapView::inverse(Projection p, double x, double y, double& lat, double& lon) {
    if (p == Projection::Robinson) {
        const double v = std::abs(y) / robinsonY;
        if (v > 1.0) {
            return false;
        }
        int i = 0;   // the table's rows are in rising order of height: the row the distance falls in
        while (i < 17 && robinsonHeight[i + 1] < v) {
            i++;
        }
        const double span = robinsonHeight[i + 1] - robinsonHeight[i];
        const double a = (i + (span > 0 ? (v - robinsonHeight[i]) / span : 0.0)) * 5.0;
        lat = a * (y < 0 ? -1.0 : 1.0);
        const double length = robinsonAt(robinsonLength, lat);
        lon = x / (robinsonX * length) / rad;
        return std::abs(lon) <= 180.0;
    }
    if (p == Projection::Mollweide) {
        const double s = y / std::sqrt(2.0);
        if (std::abs(s) > 1.0) {
            return false;
        }
        const double theta = std::asin(s);
        lat = std::asin((2.0 * theta + std::sin(2.0 * theta)) / pi) / rad;
        const double c = std::cos(theta);
        if (c < 1e-9) {
            lon = 0.0;
            return true;
        }
        lon = pi * x / (2.0 * std::sqrt(2.0) * c) / rad;
        return std::abs(lon) <= 180.0;
    }
    double theta = y / A1;   // Newton's method on y = theta (A1 + A2 theta^2 + A3 theta^6 + A4 theta^8)
    for (int i = 0; i < 12; i++) {
        const double t2 = theta * theta, t6 = t2 * t2 * t2;
        const double f = theta * (A1 + A2 * t2 + A3 * t6 + A4 * t6 * t2) - y;
        const double d = A1 + 3.0 * A2 * t2 + 7.0 * A3 * t6 + 9.0 * A4 * t6 * t2;
        theta -= f / d;
    }
    if (std::abs(theta) > pi / 3.0 + 1e-6) {
        return false;
    }
    const double t2 = theta * theta, t6 = t2 * t2 * t2;
    lat = std::asin(std::sin(theta) / M) / rad;
    lon = 3.0 * x * (9.0 * A4 * t6 * t2 + 7.0 * A3 * t6 + 3.0 * A2 * t2 + A1) / (2.0 * std::sqrt(3.0) * std::cos(theta)) / rad;
    return std::abs(lon) <= 180.0;
}

// the world as one equal-area picture, from a skin that holds the whole world (Mercator, 85 degrees south to north) centred on centerLon
QImage GlobeMapView::projectWorld(const Skin& skin, Projection p, double centerLon, int width) {
    double halfW, halfH;
    extent(p, halfW, halfH);
    const int height = static_cast<int>(std::lround(width * halfH / halfW));
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const double scale = width / (2.0 * halfW);
    const double toPixels = skin.width / 1000.0;
    const double skinCentre = (skin.west + skin.east) / 2.0;
    for (int py = 0; py < height; py++) {
        auto * row = reinterpret_cast<QRgb *>(image.scanLine(py));
        const double y = (height / 2.0 - py - 0.5) / scale;
        for (int px = 0; px < width; px++) {
            const double x = (px + 0.5 - width / 2.0) / scale;
            double lat, lonRelative;
            if (!inverse(p, x, y, lat, lonRelative)) {
                continue;
            }
            QRgb color = skin.image.pixel(0, 0) | 0xff000000u;
            if (lat > -85.0 && lat < 85.0) {
                double lon = centerLon + lonRelative;
                while (lon - skinCentre > 180.0) {
                    lon -= 360.0;
                }
                while (lon - skinCentre < -180.0) {
                    lon += 360.0;
                }
                const auto units = skin.transform(lat, lon);
                const int sx = static_cast<int>((units.x() + 500.0) * toPixels), sy = static_cast<int>((units.y() + 250.0) * toPixels);
                if (sx >= 0 && sy >= 0 && sx < skin.width && sy < skin.height) {
                    const QRgb s = reinterpret_cast<const QRgb *>(skin.image.constScanLine(sy))[sx];
                    if (qAlpha(s) > 0) {
                        color = s;
                    }
                }
            }
            row[px] = color | 0xff000000u;
        }
    }
    return image;
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
    skin = makeSkin(south, north, west, east, pixels, radius() * rad);   // the pixels a degree is on the screen, at the middle of the globe
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
