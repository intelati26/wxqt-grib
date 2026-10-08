// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsChart.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include "ui/WindBarb.h"

namespace GfsChart {
namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double msToKnots = 1.943844;

    double mercatorY(double lat) {
        const double clamped = std::clamp(lat, -82.0, 82.0) * pi / 180.0;
        return std::log(std::tan(pi / 4.0 + clamped / 2.0));
    }
    double mercatorLat(double y) {
        return (2.0 * std::atan(std::exp(y)) - pi / 2.0) * 180.0 / pi;
    }

    // The view: longitude is linear across the width, latitude is Mercator down the height.
    struct View {
        const Sector& sector;
        QRectF area;
        double yTop, yBottom;
        View(const Sector& s, const QRectF& a) : sector{s}, area{a}, yTop{mercatorY(s.north)}, yBottom{mercatorY(s.south)} {}
        QPointF toPixel(double lon, double lat) const {
            double dx = lon - sector.west;
            while (dx < -180.0) {   // only a point a half turn out is moved: a hair beyond the edge stays beyond it
                dx += 360.0;
            }
            while (dx > 540.0) {
                dx -= 360.0;
            }
            return {area.left() + dx / (sector.east - sector.west) * area.width(), area.top() + (yTop - mercatorY(lat)) / (yTop - yBottom) * area.height()};
        }
        double lonAt(double x) const { return sector.west + (x - area.left()) / area.width() * (sector.east - sector.west); }
        double latAt(double y) const { return mercatorLat(yTop - (y - area.top()) / area.height() * (yTop - yBottom)); }
        // the height the area needs so that a degree is as long both ways at the center of the view
        static double heightFor(const Sector& s, double width) {
            return width * (mercatorY(s.north) - mercatorY(s.south)) / ((s.east - s.west) * pi / 180.0);
        }
    };

    Ramp temperature() {
        return {{{-45, QColor{"#4b2d7f"}}, {-30, QColor{"#5b55a8"}}, {-20, QColor{"#3f74b8"}}, {-10, QColor{"#4aa0d4"}}, {0, QColor{"#9ad6e6"}}, {5, QColor{"#bfe4cf"}},
                 {10, QColor{"#dcefb4"}}, {15, QColor{"#f3f0a4"}}, {20, QColor{"#f8d97e"}}, {25, QColor{"#f5ac5c"}}, {30, QColor{"#e57741"}}, {35, QColor{"#c5432e"}}, {40, QColor{"#8e1f2c"}}, {47, QColor{"#5c1024"}}}};
    }
    Ramp windSpeed() {
        return {{{0, QColor{"#f7fbff"}}, {15, QColor{"#e0ecf6"}}, {30, QColor{"#c4dcee"}}, {45, QColor{"#98c5e0"}}, {60, QColor{"#6aa9d2"}}, {80, QColor{"#4687bf"}},
                 {100, QColor{"#7a72b8"}}, {120, QColor{"#b36aa6"}}, {140, QColor{"#d6618a"}}, {170, QColor{"#e8665d"}}, {200, QColor{"#f29a4d"}}}};
    }
    Ramp vorticityRamp() {   // units of 1e-5 per second; the cyclonic side only
        return {{{0, QColor{255, 255, 255, 0}}, {6, QColor{"#fff7d1"}}, {10, QColor{"#fde8a0"}}, {16, QColor{"#fcc967"}}, {24, QColor{"#f59d47"}}, {34, QColor{"#df6a34"}}, {48, QColor{"#a8402a"}}, {70, QColor{"#6b1f2a"}}}};
    }

    GfsGrid::Grid pick(const Grids& g, const std::string& key) {
        const auto found = g.find(key);
        return found == g.end() ? GfsGrid::Grid{} : found->second;
    }

    GfsData::Want want(const char * key, const char * variable, const char * level) {
        return {key, variable, level, ""};
    }
}

QRgb Ramp::at(double value) const {
    if (stops.empty() || std::isnan(value)) {
        return qRgba(0, 0, 0, 0);
    }
    if (value <= stops.front().first) {
        return stops.front().second.rgba();
    }
    for (size_t i = 1; i < stops.size(); i++) {
        if (value <= stops[i].first) {
            const double t = (value - stops[i - 1].first) / (stops[i].first - stops[i - 1].first);
            const auto a = stops[i - 1].second, b = stops[i].second;
            return qRgba(static_cast<int>(a.red() + (b.red() - a.red()) * t), static_cast<int>(a.green() + (b.green() - a.green()) * t),
                         static_cast<int>(a.blue() + (b.blue() - a.blue()) * t), static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t));
        }
    }
    return stops.back().second.rgba();
}

const std::vector<Sector>& sectors() {
    static const std::vector<Sector> list{
        {"CONUS", -127, 22, -65, 51},
        {"NAMER", -170, 5, -50, 72},
        {"SAMER", -95, -58, -30, 17},
        {"AFRICA", -25, -38, 62, 40},
        {"NORTH-PAC", 110, 0, 250, 62},
        {"EAST-PAC", -160, 0, -75, 50},
        {"WEST-ATL", -100, 5, -40, 50},
        {"ATLANTIC", -100, -5, 20, 65},
        {"POLAR", -180, 55, 180, 82},
        {"ALASKA", -175, 50, -125, 72},
        {"EUROPE", -25, 33, 50, 72},
        {"ASIA", 55, 0, 150, 60},
        {"SOUTH-PAC", 140, -60, 290, 5},
        {"ARTIC", -180, 60, 180, 82},
        {"INDIA", 50, -10, 110, 40},
        {"US-SAMOA", -190, -30, -140, 5},
        // the world
        {"GLOBAL", -180, -75, 180, 75},
        {"TROPICS", -180, -38, 180, 38},
        {"NORTHERN-HEMI", -180, 15, 180, 82},
        {"SOUTHERN-HEMI", -180, -82, 180, -15},
        {"AUSTRALIA", 105, -50, 180, -5},
        {"MIDDLE-EAST", 25, 8, 72, 45},
        {"INDIAN-OCEAN", 30, -45, 125, 28},
        {"E-ASIA", 95, 15, 150, 56},
        {"SE-ASIA", 88, -12, 150, 30},
        {"NORTH-ATL", -85, 10, 20, 68},
        {"CARIBBEAN", -92, 6, -56, 29},
        {"GULF-MEXICO", -100, 17, -78, 32},
        {"CENT-AMER", -95, 5, -70, 22},
        {"HAWAII", -165, 15, -150, 26},
        // the United States by region
        {"NORTHEAST", -83, 36, -66, 48},
        {"MID-ATLANTIC", -86, 34, -72, 42},
        {"SOUTHEAST", -93, 24, -74, 38},
        {"GREAT-LAKES", -96, 40, -74, 50},
        {"OHIO-VALLEY", -92, 34, -78, 43},
        {"S-PLAINS", -107, 25, -90, 40},
        {"N-PLAINS", -113, 39, -94, 50},
        {"ROCKIES", -118, 35, -101, 50},
        {"SOUTHWEST", -122, 30, -102, 42},
        {"PACIFIC-NW", -128, 41, -113, 52},
        {"CALIFORNIA", -126, 31, -112, 43},
        {"GULF-COAST", -100, 24, -80, 35}};
    return list;
}

const Sector * sector(const std::string& id) {
    for (const auto& s : sectors()) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

const std::vector<Product>& products() {
    static const std::vector<Product> list = [] {
        std::vector<Product> p;
        const auto speedOf = [] (const char * u, const char * v) {
            return [u, v] (const Grids& g) { return GfsGrid::scaled(GfsGrid::speed(pick(g, u), pick(g, v)), msToKnots); };
        };
        // the upper-air charts: wind speed filled, heights contoured, barbs
        const auto upper = [&] (const char * id, const char * label, const char * level, double interval) {
            Product x;
            x.id = id;
            x.label = label;
            x.wants = {want("z", "HGT", level), want("u", "UGRD", level), want("v", "VGRD", level)};
            x.fill = speedOf("u", "v");
            x.ramp = windSpeed();
            x.fillTitle = "Wind speed (kt)";
            x.legendStep = 20;
            x.contourKey = "z";
            x.contourScale = 0.1;
            x.contourInterval = interval;
            x.contourTitle = "Height (dam)";
            x.barbU = "u";
            x.barbV = "v";
            return x;
        };
        p.push_back(upper("200_wnd_ht", "200mb Wind and Height", "200 mb", 12));
        p.push_back(upper("250_wnd_ht", "250mb Wind and Height", "250 mb", 12));
        p.push_back(upper("300_wnd_ht", "300mb Wind and Height", "300 mb", 12));
        p.push_back(upper("500_wnd_ht", "500mb Wind and Height", "500 mb", 6));
        {
            auto x = upper("500_vort_ht", "500mb Vorticity, Wind, and Height", "500 mb", 6);
            x.fill = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::vorticity(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1)), 1e5); };
            x.ramp = vorticityRamp();
            x.fillTitle = "Relative vorticity (1e-5 /s)";
            x.legendStep = 10;
            p.push_back(x);
        }
        {
            auto x = upper("850_vort_ht", "850mb Vorticity, Wind and Height", "850 mb", 3);
            x.fill = [] (const Grids& g) { return GfsGrid::scaled(GfsGrid::vorticity(GfsGrid::smoothed(pick(g, "u"), 1), GfsGrid::smoothed(pick(g, "v"), 1)), 1e5); };
            x.ramp = vorticityRamp();
            x.fillTitle = "Relative vorticity (1e-5 /s)";
            x.legendStep = 10;
            p.push_back(x);
        }
        // the lower levels: temperature filled
        const auto lowTemp = [&] (const char * id, const char * label, const char * level, double interval) {
            Product x = upper(id, label, level, interval);
            x.wants.push_back(want("t", "TMP", level));
            x.fill = [] (const Grids& g) { return pick(g, "t"); };
            x.ramp = temperature();
            x.fillTitle = "Temperature";
            x.legendStep = 5;
            x.fahrenheitAware = true;
            return x;
        };
        p.push_back(lowTemp("850_temp_ht", "850mb Temperature, Wind and Height", "850 mb", 3));
        p.push_back(lowTemp("925_temp_ht", "925mb Temperature, Wind and Height", "925 mb", 3));
        // the surface: temperature, 10 m wind, sea level pressure
        {
            Product x;
            x.id = "10m_wnd_2m_temp";
            x.label = "MSLP, 10m wind, 2m temperature";
            x.wants = {want("t", "TMP", "2 m above ground"), want("u", "UGRD", "10 m above ground"), want("v", "VGRD", "10 m above ground"), want("p", "PRMSL", "mean sea level")};
            x.fill = [] (const Grids& g) { return pick(g, "t"); };
            x.ramp = temperature();
            x.fillTitle = "2 m temperature";
            x.legendStep = 5;
            x.fahrenheitAware = true;
            x.contourKey = "p";
            x.contourScale = 0.01;
            x.contourInterval = 4;
            x.contourTitle = "Sea level pressure (mb)";
            x.highsAndLows = true;
            x.barbU = "u";
            x.barbV = "v";
            p.push_back(x);
        }
        return p;
    }();
    return list;
}

const Product * product(const std::string& id) {
    for (const auto& p : products()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

QImage render(const Product& product, const Sector& sector, const Grids& grids, const GfsData::Run& run, int forecastHour, const Options& options) {
    for (const auto& w : product.wants) {
        if (grids.find(w.key) == grids.end()) {
            return {};
        }
    }
    const int headerHeight = 54, legendHeight = 62, margin = 8;
    const QRectF mapArea{static_cast<double>(margin), static_cast<double>(headerHeight), static_cast<double>(options.width - 2 * margin), 0.0};
    const double mapHeight = std::clamp(View::heightFor(sector, mapArea.width()), 200.0, 1400.0);
    const QRectF area{mapArea.left(), mapArea.top(), mapArea.width(), mapHeight};
    View view{sector, area};
    QImage image(options.width, static_cast<int>(headerHeight + mapHeight + legendHeight), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor{250, 250, 250});

    // the fill, a pixel at a time
    const auto fill = product.fill(grids);
    const auto convert = [&] (double value) { return product.fahrenheitAware && options.fahrenheit ? value * 1.8 + 32.0 : value; };
    const Ramp * ramp = &product.ramp;
    Ramp shown;
    if (product.fahrenheitAware && options.fahrenheit) {   // the same colors against Fahrenheit values
        for (const auto& stop : product.ramp.stops) {
            shown.stops.emplace_back(convert(stop.first), stop.second);
        }
        ramp = &shown;
    }
    QImage map(static_cast<int>(area.width()), static_cast<int>(area.height()), QImage::Format_ARGB32_Premultiplied);
    map.fill(QColor{244, 244, 244});
    for (int y = 0; y < map.height(); y++) {
        auto * row = reinterpret_cast<QRgb *>(map.scanLine(y));
        const double lat = view.latAt(area.top() + y + 0.5);
        for (int x = 0; x < map.width(); x++) {
            const float value = fill.sample(view.lonAt(area.left() + x + 0.5), lat);
            if (!std::isnan(value)) {
                const QRgb c = ramp->at(convert(value));
                if (qAlpha(c) == 255) {
                    row[x] = qPremultiply(c);
                } else if (qAlpha(c) > 0) {   // a ramp that fades in over the grey
                    const int a = qAlpha(c);
                    const QRgb under = row[x];
                    row[x] = qRgb((qRed(c) * a + qRed(under) * (255 - a)) / 255, (qGreen(c) * a + qGreen(under) * (255 - a)) / 255, (qBlue(c) * a + qBlue(under) * (255 - a)) / 255);
                }
            }
        }
    }
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing);
    p.drawImage(area.topLeft(), map);
    p.setClipRect(area);

    // coastlines and borders
    p.setPen(QPen{QColor{40, 40, 40, 210}, 0.9});
    for (const auto& line : options.lines) {
        QPainterPath path;
        bool started = false;
        for (const auto& pt : line) {
            if (pt.second < sector.south - 3 || pt.second > sector.north + 3) {
                started = false;
                continue;
            }
            // a longitude equivalent to the view's: the lines are -180..180, the view may reach past either
            double lon = pt.first;
            while (lon < sector.west - 25.0) {
                lon += 360.0;
            }
            while (lon >= sector.west + 335.0) {
                lon -= 360.0;
            }
            if (lon > sector.east + 25.0) {   // far off the right edge (the far side of the dateline, say): a line to it would run across the chart
                started = false;
                continue;
            }
            const auto at = view.toPixel(lon, pt.second);
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
        }
        p.drawPath(path);
    }

    QFont font = p.font();
    font.setPixelSize(11);
    font.setBold(true);
    p.setFont(font);
    const QFontMetricsF metrics{font};
    const auto halo = [&] (const QPointF& at, const QString& text, const QColor& color) {   // a label with a light edge so it reads over the colors
        const QPointF origin{at.x() - metrics.horizontalAdvance(text) / 2.0, at.y() + metrics.ascent() / 2.0 - 1};
        QPainterPath path;
        path.addText(origin, font, text);
        p.setPen(QPen{QColor{255, 255, 255, 220}, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPath(path);
    };

    // the wider the view, the sparser the lines, the highs and lows, and the barbs
    const double span = sector.east - sector.west;
    const double interval = product.contourInterval * (span > 150.0 ? 2.0 : 1.0);
    const double extremeRadius = span > 150.0 ? 12.0 : span > 80.0 ? 8.0 : 6.0;
    // contours
    if (!product.contourKey.empty() && interval > 0.0) {
        const auto& grid = grids.at(product.contourKey);
        double lo = 1e18, hi = -1e18;
        for (int y = 0; y < map.height(); y += 6) {
            for (int x = 0; x < map.width(); x += 6) {
                const float value = grid.sample(view.lonAt(area.left() + x), view.latAt(area.top() + y));
                if (!std::isnan(value)) {
                    lo = std::min<double>(lo, value * product.contourScale);
                    hi = std::max<double>(hi, value * product.contourScale);
                }
            }
        }
        const double first = std::ceil((lo - product.contourBase) / interval) * interval + product.contourBase;
        const double west = sector.west, east = sector.east;
        for (double level = first; level <= hi; level += interval) {
            const bool heavy = std::fmod(std::abs(level - product.contourBase), interval * 5) < 1e-6;
            for (const auto& line : GfsGrid::contour(grid, level / product.contourScale, west, sector.south, east, sector.north)) {
                QPainterPath path;
                bool outside = true;
                for (const auto& pt : line) {
                    const auto at = view.toPixel(pt.lon, pt.lat);   // already continuous across west..east
                    outside ? path.moveTo(at) : path.lineTo(at);
                    outside = false;
                }
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen{QColor{30, 30, 30, 230}, heavy ? 1.6 : 1.0});
                p.drawPath(path);
                // one label near the middle of a line long enough to carry it
                if (path.length() > 150.0) {
                    const auto mid = path.pointAtPercent(0.5);
                    if (area.adjusted(24, 14, -24, -14).contains(mid)) {
                        halo(mid, QString::number(static_cast<int>(std::lround(level))), QColor{20, 20, 20});
                    }
                }
            }
        }
        if (product.highsAndLows) {
            for (const auto& e : GfsGrid::extremes(grid, extremeRadius, west, sector.south, east, sector.north)) {
                const auto at = view.toPixel(e.lon, e.lat);
                if (area.adjusted(20, 20, -20, -20).contains(at)) {
                    halo(at, e.high ? "H" : "L", e.high ? QColor{20, 60, 170} : QColor{190, 30, 30});
                    halo(at + QPointF{0, 13}, QString::number(static_cast<int>(std::lround(e.value * product.contourScale))), QColor{40, 40, 40});
                }
            }
        }
    }

    // wind barbs on a grid of pixels
    if (!product.barbU.empty()) {
        const auto& u = grids.at(product.barbU);
        const auto& v = grids.at(product.barbV);
        const double spacing = span > 150.0 ? 62.0 : span > 80.0 ? 52.0 : 48.0;
        p.setPen(QPen{QColor{20, 20, 20, 230}, 1.0});
        p.setBrush(QColor{20, 20, 20});
        for (double y = area.top() + spacing / 2; y < area.bottom(); y += spacing) {
            for (double x = area.left() + spacing / 2; x < area.right(); x += spacing) {
                const double lon = view.lonAt(x), lat = view.latAt(y);
                const float uu = u.sample(lon, lat), vv = v.sample(lon, lat);
                if (std::isnan(uu) || std::isnan(vv)) {
                    continue;
                }
                const double from = std::fmod(std::atan2(-uu, -vv) * 180.0 / pi + 360.0, 360.0);
                WindBarb::draw(p, {x, y}, from, std::hypot(uu, vv) * msToKnots, 24.0, lat < 0.0);
            }
        }
    }
    p.setClipping(false);
    p.setPen(QPen{QColor{60, 60, 60}, 1.0});
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);

    // header
    const auto init = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
    auto initUtc = QDateTime{init.date(), init.time(), Qt::UTC};
    const auto valid = initUtc.addSecs(forecastHour * 3600);
    QFont big = font;
    big.setPixelSize(17);
    p.setFont(big);
    p.setPen(QColor{25, 25, 25});
    p.drawText(QPointF{static_cast<double>(margin), 22.0}, QString::fromStdString("GFS  " + product.label));
    font.setBold(false);
    p.setFont(font);
    p.setPen(QColor{70, 70, 70});
    p.drawText(QPointF{static_cast<double>(margin), 43.0},
               QString::fromStdString(product.fillTitle) + (product.fahrenheitAware ? (options.fahrenheit ? " (\u00b0F)" : " (\u00b0C)") : "") + (product.contourTitle.empty() ? "" : QString::fromStdString(";  lines: " + product.contourTitle)) +
                   (product.barbU.empty() ? "" : ";  barbs: kt"));
    const QString times = "Run " + initUtc.toString("ddd dd MMM yyyy HH") + "Z   F" + QString::number(forecastHour).rightJustified(3, '0') + "   Valid " + valid.toString("ddd dd MMM HH") + "Z";
    p.drawText(QRectF{0, 8, image.width() - static_cast<double>(margin), 20}, Qt::AlignRight, times);

    // legend
    const double barLeft = margin + 6.0, barTop = area.bottom() + 12.0, barWidth = area.width() - 12.0, barHeight = 16.0;
    const double first = ramp->stops.front().first, last = ramp->stops.back().first;
    for (int x = 0; x < static_cast<int>(barWidth); x++) {
        const double value = first + (last - first) * x / barWidth;
        const QRgb c = ramp->at(value);
        p.fillRect(QRectF{barLeft + x, barTop, 1.5, barHeight}, QColor{qRed(c), qGreen(c), qBlue(c), qAlpha(c) == 0 ? 0 : 255});
    }
    p.setPen(QColor{60, 60, 60});
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF{barLeft, barTop, barWidth, barHeight});
    for (double value = std::ceil(first / product.legendStep) * product.legendStep; value <= last; value += product.legendStep) {
        const double x = barLeft + (value - first) / (last - first) * barWidth;
        p.drawLine(QPointF{x, barTop + barHeight}, QPointF{x, barTop + barHeight + 4});
        p.drawText(QRectF{x - 22, barTop + barHeight + 4, 44, 14}, Qt::AlignHCenter, QString::number(value, 'f', 0));
    }
    p.setPen(QColor{110, 110, 110});
    p.drawText(QRectF{0, image.height() - 16.0, image.width() - static_cast<double>(margin), 14}, Qt::AlignRight, "Data: NOAA/NCEP GFS 0.25 degree");
    return image;
}
}
