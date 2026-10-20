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
    return p == Projection::Globe ? "Globe" : p == Projection::Robinson ? "Robinson" : p == Projection::EqualEarth ? "Equal Earth (equal area)" : p == Projection::PolarNorth ? "North polar" : p == Projection::PolarSouth ? "South polar" : "Mollweide (equal area)";
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

namespace {
    double polarLimit = 20.0;   // degrees of latitude the sides of the polar maps reach to
    bool polarWideFrame = false;
    // the stereographic radius (the sphere of radius 1, the pole at the middle) of a latitude on the north (+1) or the south (-1) polar map
    double polarRadius(double lat, int hemisphere) {
        return 2.0 * std::tan((90.0 - hemisphere * lat) * 3.14159265358979 / 360.0);
    }
}

void GlobeMapView::setPolarFrame(double reachLatitude, bool wide) {
    polarLimit = std::clamp(reachLatitude, 0.0, 70.0);
    polarWideFrame = wide;
}

bool GlobeMapView::polarWide() {
    return polarWideFrame;
}

void GlobeMapView::extent(Projection p, double& halfWidth, double& halfHeight) {
    if (p == Projection::PolarNorth || p == Projection::PolarSouth) {
        halfWidth = polarRadius(polarLimit, 1);
        halfHeight = polarWideFrame ? halfWidth * 9.0 / 16.0 : halfWidth;
    } else if (p == Projection::Robinson) {
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
    if (p == Projection::PolarNorth || p == Projection::PolarSouth) {   // the meridian of the centre points down; east runs counter-clockwise in the north, clockwise in the south
        const int hemisphere = p == Projection::PolarNorth ? 1 : -1;
        const double r = polarRadius(lat, hemisphere);
        x = hemisphere * r * std::sin(lambda);
        y = -r * std::cos(lambda);
        return true;
    }
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
    if (p == Projection::PolarNorth || p == Projection::PolarSouth) {
        const int hemisphere = p == Projection::PolarNorth ? 1 : -1;
        const double r = std::hypot(x, y);
        if (!polarWideFrame && r > polarRadius(polarLimit, 1) + 1e-9) {
            return false;
        }
        lat = hemisphere * (90.0 - 2.0 * std::atan(r / 2.0) / rad);
        lon = r < 1e-12 ? 0.0 : std::atan2(hemisphere * x, -y) / rad;
        return true;
    }
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
            if (p == Projection::PolarNorth || p == Projection::PolarSouth) {
                lat = std::clamp(lat, -83.5, 83.5);   // (the skin is Mercator and holds nothing in its last degrees: the cap at the pole takes the row at 83.5)
            }
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

void GlobeMapView::setPreview(const std::string& name, double west, double south, double east, double north) {
    previewOn = true;
    previewName = name;
    pWest = west;
    pSouth = south;
    pEast = east;
    pNorth = north;
    update();
}

void GlobeMapView::clearPreview() {
    previewOn = false;
    update();
}

double GlobeMapView::zoomFor(double west, double south, double east, double north) {
    // the angle from the middle of the area to its corner, on the sphere: the area fits when that corner is at 0.9 of the radius
    const double lat = (south + north) / 2.0, lon = (west + east) / 2.0;
    const double phi = lat * rad, phi2 = north * rad;
    const double dl = (east - lon) * rad;
    const double cosc = std::sin(phi) * std::sin(phi2) + std::cos(phi) * std::cos(phi2) * std::cos(dl);
    const double angle = std::acos(std::clamp(cosc, -1.0, 1.0));
    return std::clamp(0.92 / std::max(0.1, std::sin(std::min(angle, 1.45))), 0.8, 12.0);
}

void GlobeMapView::rotateTo(double lat, double lon, double newZoom) {
    fromLat = lat0;
    fromLon = lon0;
    fromZoom = zoom;
    toLat = std::clamp(lat, -89.0, 89.0);
    toLon = lon0 + std::fmod(lon - lon0 + 540.0, 360.0) - 180.0;   // the short way round
    toZoom = newZoom > 0.0 ? newZoom : zoom;
    turnStep = 0;
    turnTimer.start(16);
}

void GlobeMapView::setAreas(const std::vector<Area>& list, const std::function<void(const std::string&)>& chosen) {
    areas = list;
    areaChosen = chosen;
    areaUnder.clear();
    if (list.empty()) {
        clearPreview();
    }
}

GlobeMapView::GlobeMapView(QWidget * parent) : QWidget{parent} {
    turnTimer.setSingleShot(false);
    QObject::connect(&turnTimer, &QTimer::timeout, this, [this] {
        constexpr int steps = 16;
        turnStep++;
        const double f = turnStep >= steps ? 1.0 : 0.5 - 0.5 * std::cos(3.14159265358979 * turnStep / steps);   // slow at both ends
        lat0 = fromLat + (toLat - fromLat) * f;
        lon0 = fromLon + (toLon - fromLon) * f;
        zoom = fromZoom + (toZoom - fromZoom) * f;
        while (lon0 > 180.0) {
            lon0 -= 360.0;
        }
        while (lon0 < -180.0) {
            lon0 += 360.0;
        }
        if (turnStep >= steps) {
            turnTimer.stop();
            scheduleSkin(80);
        }
        update();
    });
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
    if (previewOn) {   // the area: its outline on the sphere (the front side only), filled when all of it is in sight, with its name
        const double R = radius(), phi0 = lat0 * rad;
        const auto project = [&] (double lat, double lon, QPointF& out) {
            const double dl = (lon - lon0) * rad, phi = lat * rad;
            if (std::sin(phi0) * std::sin(phi) + std::cos(phi0) * std::cos(phi) * std::cos(dl) <= 0.0) {
                return false;
            }
            out = center() + QPointF{R * std::cos(phi) * std::sin(dl), -R * (std::cos(phi0) * std::sin(phi) - std::sin(phi0) * std::cos(phi) * std::cos(dl))};
            return true;
        };
        QPolygonF outline;
        bool whole = true;
        const auto edge = [&] (double fromLatE, double fromLonE, double toLatE, double toLonE) {
            const int n = std::max(8, static_cast<int>(std::max(std::abs(toLatE - fromLatE), std::abs(toLonE - fromLonE))));
            for (int i = 0; i < n; i++) {
                QPointF at;
                if (project(fromLatE + (toLatE - fromLatE) * i / n, fromLonE + (toLonE - fromLonE) * i / n, at)) {
                    outline << at;
                } else {
                    whole = false;
                }
            }
        };
        edge(pSouth, pWest, pSouth, pEast);
        edge(pSouth, pEast, pNorth, pEast);
        edge(pNorth, pEast, pNorth, pWest);
        edge(pNorth, pWest, pSouth, pWest);
        if (outline.size() >= 2) {
            p.setPen(QPen{QColor{230, 40, 40}, 2.2});
            p.setBrush(whole ? QColor{230, 40, 40, 45} : QColor{});
            whole ? p.drawPolygon(outline) : p.drawPolyline(outline);
        }
        QPointF middle;
        if (project((pSouth + pNorth) / 2.0, (pWest + pEast) / 2.0, middle) && !previewName.empty()) {
            QFont font = p.font();
            font.setPointSizeF(font.pointSizeF() * 1.15);
            p.setFont(font);
            const QString text = QString::fromStdString(previewName);
            const QRectF box = QRectF{0, 0, p.fontMetrics().horizontalAdvance(text) + 16.0, p.fontMetrics().height() + 8.0}.translated(middle - QPointF{(p.fontMetrics().horizontalAdvance(text) + 16.0) / 2.0, p.fontMetrics().height() + 14.0});
            p.setPen(QPen{QColor{235, 235, 235}, 1.0});
            p.setBrush(QColor{15, 15, 15, 225});
            p.drawRoundedRect(box, 4, 4);
            p.setPen(QColor{245, 245, 245});
            p.drawText(box, Qt::AlignCenter, text);
        }
    }
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
    } else if (!areas.empty() && !areaUnder.empty() && areaChosen) {   // a click on an area: chosen
        areaChosen(areaUnder);
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
    if (!areas.empty()) {   // the smallest area the pointer is over, previewed
        std::string found;
        double best = 1e18;
        if (place(e->position(), lat, lon)) {
            for (const auto& a : areas) {
                double relative = lon - (a.west + a.east) / 2.0;
                while (relative > 180.0) relative -= 360.0;
                while (relative < -180.0) relative += 360.0;
                const double size = (a.east - a.west) * (a.north - a.south);
                if (std::abs(relative) <= (a.east - a.west) / 2.0 && lat >= a.south && lat <= a.north && size < best) {
                    best = size;
                    found = a.id;
                }
            }
        }
        if (found != areaUnder) {
            areaUnder = found;
            if (found.empty()) {
                clearPreview();
            } else {
                for (const auto& a : areas) {
                    if (a.id == found) {
                        setPreview(a.id, a.west, a.south, a.east, a.north);
                    }
                }
            }
        }
        setCursor(found.empty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
        return;
    }
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

// Lines of latitude every 10 degrees and of longitude every 30, over a picture of the world, with their numbers where they leave the map (a polar map: the latitudes along the meridian a quarter
// turn from the one that points down, the longitudes at the edge of the circle or the frame).
void GlobeMapView::drawGraticule(QPainter& painter, Projection p, double centerLon, int width, int height, double marks) {
    double halfW, halfH;
    extent(p, halfW, halfH);
    const double scale = width / (2.0 * halfW);
    const auto pixel = [&] (double lat, double relativeLon, QPointF& out) {
        double x, y;
        forward(p, lat, relativeLon, x, y);
        out = QPointF{width / 2.0 + x * scale, height / 2.0 - y * scale};
    };
    const auto inside = [&] (const QPointF& at) {
        if (p == Projection::PolarNorth || p == Projection::PolarSouth) {
            if (!polarWideFrame) {
                return std::hypot(at.x() - width / 2.0, at.y() - height / 2.0) <= width / 2.0 - 1.0;
            }
        }
        return at.x() >= 1.0 && at.y() >= 1.0 && at.x() <= width - 1.0 && at.y() <= height - 1.0;
    };
    const auto relative = [&] (double lon) {
        double r = lon - centerLon;
        while (r > 180.0) r -= 360.0;
        while (r < -180.0) r += 360.0;
        return r;
    };
    const bool polar = isPolar(p);
    const int hemisphere = p == Projection::PolarNorth ? 1 : -1;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font{painter.font()};
    font.setPixelSize(std::max(11, static_cast<int>(15 * marks)));
    painter.setFont(font);
    const QPen line{QColor{40, 40, 60, 110}, std::max(1.0, 0.8 * marks)};
    painter.setPen(line);
    painter.setBrush(Qt::NoBrush);
    // the parallels
    for (int lat = -80; lat <= 80; lat += 10) {
        if (polar && lat * hemisphere <= 0) {
            continue;
        }
        QPainterPath path;
        bool open = false;
        for (double lon = -180.0; lon <= 180.0; lon += 2.0) {
            QPointF at;
            pixel(lat, lon, at);
            if (!inside(at)) {
                open = false;
                continue;
            }
            open ? path.lineTo(at) : path.moveTo(at);
            open = true;
        }
        painter.setPen(lat == 0 ? QPen{QColor{40, 40, 60, 160}, std::max(1.0, 1.2 * marks)} : line);
        painter.drawPath(path);
    }
    // the meridians
    for (int lon = -180; lon < 180; lon += 30) {
        QPainterPath path;
        bool open = false;
        QPointF lastInside;
        bool any = false;
        for (int k = 0; k <= 90; k++) {   // from the pole outward
            const double lat = polar ? hemisphere * (89.9 - k) : std::min(85.0, -85.0 + k * 2.0);
            QPointF at;
            pixel(lat, relative(lon), at);
            if (!inside(at)) {
                open = false;
                continue;
            }
            open ? path.lineTo(at) : path.moveTo(at);
            open = true;
            lastInside = at;
            any = true;
        }
        painter.setPen(line);
        painter.drawPath(path);
        if (polar && any) {   // the longitude where the meridian leaves the map (the last point inside, drawn a little inside it)
            const QPointF centre{width / 2.0, height / 2.0};
            QPointF toward = lastInside - centre;
            const double length = std::max(1.0, std::hypot(toward.x(), toward.y()));
            const QPointF at = lastInside - toward / length * (14.0 * marks);
            const QString text = QString::number(std::abs(lon)) + QString::fromUtf8("°") + (lon == 0 ? "" : lon > 0 ? "E" : "W");
            painter.setPen(QColor{255, 255, 255, 230});
            for (const auto d : {QPointF{1, 1}, QPointF{-1, 1}, QPointF{1, -1}, QPointF{-1, -1}}) {
                painter.drawText(QRectF{at.x() - 36 * marks + d.x(), at.y() - 10 * marks + d.y(), 72 * marks, 20 * marks}, Qt::AlignCenter, text);
            }
            painter.setPen(QColor{30, 30, 50});
            painter.drawText(QRectF{at.x() - 36 * marks, at.y() - 10 * marks, 72 * marks, 20 * marks}, Qt::AlignCenter, text);
        }
    }
    // the latitudes, along a meridian between two of the drawn ones
    if (polar) {
        for (int lat = 10; lat <= 80; lat += 10) {
            QPointF at;
            pixel(hemisphere * lat, relative(centerLon + 15.0), at);
            if (!inside(at)) {
                continue;
            }
            const QString text = QString::number(lat) + QString::fromUtf8("°") + (hemisphere > 0 ? "N" : "S");
            painter.setPen(QColor{255, 255, 255, 230});
            for (const auto d : {QPointF{1, 1}, QPointF{-1, 1}, QPointF{1, -1}, QPointF{-1, -1}}) {
                painter.drawText(QRectF{at.x() - 36 * marks + d.x(), at.y() - 10 * marks + d.y(), 72 * marks, 20 * marks}, Qt::AlignCenter, text);
            }
            painter.setPen(QColor{30, 30, 50});
            painter.drawText(QRectF{at.x() - 36 * marks, at.y() - 10 * marks, 72 * marks, 20 * marks}, Qt::AlignCenter, text);
        }
    }
    painter.restore();
}
