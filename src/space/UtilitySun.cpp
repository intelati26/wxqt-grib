// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/UtilitySun.h"
#include <algorithm>
#include <cmath>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include "util/UtilityIO.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
    double rad(double d) { return d * pi / 180.0; }

    QJsonValue number(const QJsonObject& o, const char * key) { return o.value(key); }
}

double UtilitySun::tilt(const QDateTime& utc) {
    // Meeus, Astronomical Algorithms, chapter 29 (the Sun's physical ephemeris)
    const double jd = utc.toUTC().toSecsSinceEpoch() / 86400.0 + 2440587.5;
    const double t = (jd - 2451545.0) / 36525.0;
    const double l0 = std::fmod(280.46646 + 36000.76983 * t, 360.0), m = rad(std::fmod(357.52911 + 35999.05029 * t, 360.0));
    const double c = (1.914602 - 0.004817 * t) * std::sin(m) + 0.019993 * std::sin(2 * m) + 0.000289 * std::sin(3 * m);
    const double omega = rad(125.04 - 1934.136 * t);
    const double lambda = rad(l0 + c - 0.00569 - 0.00478 * std::sin(omega));
    const double k = rad(73.6667 + 1.3958333 * (jd - 2396758.0) / 36525.0);
    const double i = rad(7.25);
    return std::asin(std::sin(lambda - k) * std::sin(i)) * 180.0 / pi;
}

double UtilitySun::drift(double latitude, double days) {
    const double s = std::sin(rad(latitude));
    return (13.2 - 2.4 * s * s) * days;   // (the synodic rate of the sunspots, from the Sun's differential rotation, seen from the Earth)
}

bool UtilitySun::onDisc(double latitude, double longitudeWest, double b0, double& x, double& y) {
    const double phi = rad(latitude), lam = rad(longitudeWest), b = rad(b0);
    const double z = std::sin(phi) * std::sin(b) + std::cos(phi) * std::cos(lam) * std::cos(b);
    x = std::cos(phi) * std::sin(lam);
    y = std::sin(phi) * std::cos(b) - std::cos(phi) * std::cos(lam) * std::sin(b);
    return z >= 0.0;
}

std::vector<UtilitySun::Region> UtilitySun::parseRegions(const QByteArray& json) {
    std::vector<Region> out;
    const auto array = QJsonDocument::fromJson(json).array();
    QDate newest;
    for (const auto& v : array) {
        const auto d = QDate::fromString(v.toObject().value("observed_date").toString(), Qt::ISODate);
        if (d.isValid() && (!newest.isValid() || d > newest)) {
            newest = d;
        }
    }
    for (const auto& v : array) {
        const auto o = v.toObject();
        const auto d = QDate::fromString(o.value("observed_date").toString(), Qt::ISODate);
        if (d != newest) {
            continue;
        }
        Region r;
        r.number = o.value("region").toInt();
        r.latitude = o.value("latitude").toDouble();
        // "location": "S15W97": the longitude to the west is positive
        const auto loc = o.value("location").toString();
        const int w = loc.indexOf('W'), e = loc.indexOf('E');
        if (loc.size() < 4 || (w < 0 && e < 0)) {
            continue;
        }
        // the list's own longitude is negative to the west; the location ("S15W97") says it in words
        const int at = w >= 0 ? w : e;
        r.longitude = (w >= 0 ? 1.0 : -1.0) * loc.mid(at + 1).toDouble();
        r.area = o.value("area").isNull() ? -1 : o.value("area").toInt();
        r.spotClass = o.value("spot_class").toString().toStdString();
        r.magClass = o.value("mag_class").toString().toStdString();
        r.spots = o.value("number_spots").toInt();
        r.mProbability = o.value("m_flare_probability").toInt();
        r.xProbability = o.value("x_flare_probability").toInt();
        r.observed = d;
        out.push_back(r);
    }
    return out;
}

QImage UtilitySun::labelled(const QImage& source, const std::vector<Region>& regions, const QDateTime& when, int width) {
    if (source.isNull()) {
        return {};
    }
    const QImage disc = source.convertToFormat(QImage::Format_RGB32);
    // the disc's size from the middle row and column: the first and last bright pixels
    const auto bright = [&disc] (int x, int y) { return qGray(disc.pixel(x, y)) > 70; };
    int left = disc.width() / 2, right = left, top = disc.height() / 2, bottom = top;
    for (int x = 0; x < disc.width(); x++) {
        if (bright(x, disc.height() / 2)) { left = std::min(left, x); right = std::max(right, x); }
    }
    for (int y = 0; y < disc.height(); y++) {
        if (bright(disc.width() / 2, y)) { top = std::min(top, y); bottom = std::max(bottom, y); }
    }
    const double radius = ((right - left) + (bottom - top)) / 4.0;
    const double cx = (left + right) / 2.0, cy = (top + bottom) / 2.0;
    const double scale = static_cast<double>(width) / disc.width();
    QImage out = disc.scaled(width, static_cast<int>(disc.height() * scale), Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
    QPainter p{&out};
    p.setRenderHint(QPainter::Antialiasing);
    const double r = radius * scale, ox = cx * scale, oy = cy * scale;
    const double b0 = tilt(when);
    const auto at = [&] (double x, double y) { return QPointF{ox + x * r, oy - y * r}; };
    QFont font = p.font();
    font.setPixelSize(std::max(10, width / 40));
    p.setFont(font);
    // the grid: meridians and parallels every 30 degrees
    p.setPen(QPen{QColor{255, 255, 255, 90}, std::max(1.0, width / 500.0)});
    for (int lon = -90; lon <= 90; lon += 30) {
        QPainterPath path;
        bool pen = false;
        for (int lat = -90; lat <= 90; lat += 3) {
            double x, y;
            if (onDisc(lat, lon, b0, x, y)) {
                pen ? path.lineTo(at(x, y)) : path.moveTo(at(x, y));
                pen = true;
            } else {
                pen = false;
            }
        }
        p.drawPath(path);
    }
    for (int lat = -60; lat <= 60; lat += 30) {
        QPainterPath path;
        bool pen = false;
        for (int lon = -90; lon <= 90; lon += 3) {
            double x, y;
            if (onDisc(lat, lon, b0, x, y)) {
                pen ? path.lineTo(at(x, y)) : path.moveTo(at(x, y));
                pen = true;
            } else {
                pen = false;
            }
        }
        p.setPen(QPen{lat == 0 ? QColor{255, 255, 255, 150} : QColor{255, 255, 255, 90}, std::max(1.0, width / 500.0)});
        p.drawPath(path);
    }
    // the compass of the disc and the degrees along the equator
    const auto label = [&] (const QPointF& pos, const QString& text, Qt::Alignment align, const QColor& color) {
        const QFontMetricsF fm{p.font()};
        const double w = fm.horizontalAdvance(text), h = fm.height();
        QPointF origin = pos;
        if (align & Qt::AlignRight) origin.rx() -= w;
        if (align & Qt::AlignHCenter) origin.rx() -= w / 2;
        if (align & Qt::AlignBottom) origin.ry() -= h * 0.2; else origin.ry() += h * 0.8;
        p.setPen(QColor{0, 0, 0, 200});
        for (const auto& d : {QPointF{1, 1}, QPointF{-1, 1}, QPointF{1, -1}, QPointF{-1, -1}}) {
            p.drawText(origin + d, text);
        }
        p.setPen(color);
        p.drawText(origin, text);
    };
    label(QPointF{ox - r - 2, oy}, "E", Qt::AlignRight | Qt::AlignBottom, QColor{150, 200, 255});
    label(QPointF{ox + r + 2, oy}, "W", Qt::AlignLeft | Qt::AlignBottom, QColor{255, 190, 120});
    label(QPointF{ox, oy - r - 1}, "N", Qt::AlignHCenter | Qt::AlignBottom, QColor{255, 255, 255});
    label(QPointF{ox, oy + r + 1}, "S", Qt::AlignHCenter, QColor{255, 255, 255});
    for (const int lon : {-60, -30, 30, 60}) {
        double x, y;
        if (onDisc(0, lon, b0, x, y)) {
            label(at(x, y) + QPointF{0, -3}, QString{"%1%2"}.arg(lon < 0 ? "E" : "W").arg(std::abs(lon)), Qt::AlignHCenter | Qt::AlignBottom, QColor{255, 255, 255, 190});
        }
    }
    // the regions
    const auto observedAt = [&] (const Region& region) { return QDateTime{region.observed.addDays(1), QTime{0, 0}, QTimeZone::utc()}; };   // (the list's positions are for the end of its day)
    for (const auto& region : regions) {
        const double days = region.observed.isValid() ? observedAt(region).secsTo(when) / 86400.0 : 0.0;
        const double lon = region.longitude + drift(region.latitude, days);
        if (std::fabs(lon) > 100.0) {
            continue;
        }
        double x, y;
        const bool front = onDisc(region.latitude, std::clamp(lon, -90.0, 90.0), b0, x, y);
        const bool behind = std::fabs(lon) > 90.0 || !front;
        if (behind) {   // just over the limb: at the edge, dimmed
            const double len = std::hypot(x, y);
            if (len > 1e-6) { x /= len; y /= len; }
        }
        const QPointF pos = at(x * (behind ? 1.0 : 1.0), y);
        QColor color{255, 255, 120};
        if (region.xProbability >= 10 || region.mProbability >= 50) color = QColor{255, 70, 70};
        else if (region.mProbability >= 15) color = QColor{255, 160, 40};
        else if (region.area <= 0 && region.spots <= 0) color = QColor{210, 210, 210};
        if (behind) color.setAlpha(150);
        p.setPen(QPen{color, std::max(1.2, width / 350.0)});
        p.setBrush(Qt::NoBrush);
        const double ring = std::max(5.0, width / 70.0);
        p.drawEllipse(pos, ring, ring);
        label(pos + QPointF{ring + 2, ring * 0.3}, QString::number(region.number), Qt::AlignLeft | Qt::AlignBottom, color);
    }
    // the key
    font.setPixelSize(std::max(9, width / 55));
    p.setFont(font);
    label(QPointF{6, 4}, QString{"Active regions (AR number), grid every 30 deg, tilt B0 %1 deg"}.arg(b0, 0, 'f', 1), Qt::AlignLeft, QColor{255, 255, 255, 220});
    label(QPointF{6, 4 + font.pixelSize() * 1.3}, "East left, west right.  Red: M flare chance 50%+ or X 10%+;  orange: M 15%+", Qt::AlignLeft, QColor{255, 255, 255, 220});
    label(QPointF{6, 4 + font.pixelSize() * 2.6}, "SDO/HMI picture and region list: NOAA SWPC", Qt::AlignLeft, QColor{255, 255, 255, 170});
    return out;
}

QImage UtilitySun::labelled(int width, QDateTime * time) {
    QImage disc;
    disc.loadFromData(UtilityIO::downloadAsByteArray("https://services.swpc.noaa.gov/images/animations/sdo-hmii/latest.jpg"));
    const auto regions = parseRegions(UtilityIO::downloadAsByteArray("https://services.swpc.noaa.gov/json/solar_regions.json"));
    const auto now = QDateTime::currentDateTimeUtc();
    if (time != nullptr) {
        *time = now;
    }
    return labelled(disc, regions, now, width);
}
