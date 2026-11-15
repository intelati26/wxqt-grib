// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/VortexSection.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <QFont>
#include <QPainter>
#include <QPolygonF>
#include "mapkit/WorldRender.h"

namespace {
    // the color of a wind: white at nothing, reds westerly (the vortex), blues easterly (the reversal); the scale is the same for every level
    QColor windColor(double u, double reach) {
        if (std::isnan(u)) {
            return QColor{225, 225, 225};
        }
        const double f = std::clamp(u / reach, -1.0, 1.0);
        const QColor white{250, 250, 250}, red{190, 40, 40}, blue{40, 80, 190};
        const QColor& end = f >= 0.0 ? red : blue;
        const double k = std::abs(f);
        return QColor::fromRgbF(white.redF() + (end.redF() - white.redF()) * k, white.greenF() + (end.greenF() - white.greenF()) * k, white.blueF() + (end.blueF() - white.blueF()) * k);
    }
}

bool VortexSection::collect(const std::string& model, const std::vector<QDateTime>& when, const std::vector<int>& levels, bool south, Series& out, const std::function<bool(size_t, size_t)>& progress) {
    out = Series{};
    out.when = when;
    out.south = south;
    out.levels = levels;
    std::sort(out.levels.begin(), out.levels.end());   // (the top, the smallest pressure, first)
    out.wind.assign(out.levels.size(), std::vector<double>(when.size(), std::nan("")));
    const double latitude = south ? -60.0 : 60.0;
    for (size_t i = 0; i < when.size(); i++) {
        if (!progress(i, when.size())) {
            return false;
        }
        for (size_t l = 0; l < out.levels.size(); l++) {
            const auto sources = WorldRender::loadSources(std::to_string(out.levels[l]) + "_wnd_ht", {model}, when[i]);
            if (sources.empty() || sources.front().windU.empty()) {
                continue;
            }
            double sum = 0.0;
            int n = 0;
            for (double lon = -180.0; lon < 180.0; lon += 2.5) {
                float u = sources.front().windU.sample(lon, latitude);
                if (std::isnan(u)) {
                    u = sources.front().windU.sample(lon + 360.0, latitude);
                }
                if (!std::isnan(u)) {
                    sum += u;
                    n++;
                }
            }
            if (n > 0) {
                out.wind[l][i] = sum / n;
            }
        }
    }
    // the levels that gave nothing at any time are not shown
    for (size_t l = out.levels.size(); l > 0; l--) {
        const auto& row = out.wind[l - 1];
        if (std::all_of(row.begin(), row.end(), [] (double v) { return std::isnan(v); })) {
            out.levels.erase(out.levels.begin() + static_cast<long>(l - 1));
            out.wind.erase(out.wind.begin() + static_cast<long>(l - 1));
        }
    }
    return !out.levels.empty();
}

QImage VortexSection::draw(const Series& series, int width, const QString& title) {
    if (series.levels.empty() || series.when.empty()) {
        return {};
    }
    const double unit = width / 1200.0;
    const int left = static_cast<int>(80 * unit), right = static_cast<int>(30 * unit), top = static_cast<int>(70 * unit), bottom = static_cast<int>(100 * unit);
    const int rowHeight = static_cast<int>(70 * unit);
    const int height = top + rowHeight * static_cast<int>(series.levels.size()) + bottom;
    QImage image{width, height, QImage::Format_ARGB32};
    image.fill(Qt::white);
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing);
    QFont font = p.font();
    font.setPixelSize(std::max(10, static_cast<int>(13 * unit)));
    QFont bold = font;
    bold.setBold(true);
    bold.setPixelSize(std::max(12, static_cast<int>(17 * unit)));
    p.setFont(bold);
    p.setPen(QColor{25, 25, 25});
    p.drawText(QRectF{left, 8 * unit, width - left - right, 26 * unit}, Qt::AlignLeft | Qt::AlignVCenter, title);
    p.setFont(font);
    p.setPen(QColor{80, 80, 80});
    p.drawText(QRectF{left, 36 * unit, width - left - right, 20 * unit}, Qt::AlignLeft | Qt::AlignVCenter,
               QString{"Zonal-mean zonal wind on the circle of 60°%1 (m/s): red westerly, blue easterly (the winds have reversed)"}.arg(series.south ? "S" : "N"));
    double reach = 10.0;   // the scale holds the largest wind seen, in tens
    for (const auto& row : series.wind) {
        for (const double v : row) {
            if (!std::isnan(v)) {
                reach = std::max(reach, std::ceil(std::abs(v) / 10.0) * 10.0);
            }
        }
    }
    const double plotWidth = width - left - right;
    const size_t n = series.when.size();
    const double cell = plotWidth / static_cast<double>(n);
    for (size_t l = 0; l < series.levels.size(); l++) {
        const double y = top + static_cast<double>(l) * rowHeight;
        for (size_t i = 0; i < n; i++) {
            p.fillRect(QRectF{left + cell * static_cast<double>(i), y, cell + 0.6, static_cast<double>(rowHeight)}, windColor(series.wind[l][i], reach));
        }
        p.setPen(QColor{40, 40, 40});
        p.drawText(QRectF{4 * unit, y, left - 10 * unit, static_cast<double>(rowHeight)}, Qt::AlignRight | Qt::AlignVCenter, QString::number(series.levels[l]) + " mb");
    }
    p.setPen(QPen{QColor{120, 120, 120}, std::max(1.0, unit)});
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF{static_cast<double>(left), static_cast<double>(top), plotWidth, static_cast<double>(rowHeight * static_cast<int>(series.levels.size()))});
    // the dates: a tick at each day of a short range (three weeks or less), else at the first of each week
    const bool daily = n > 0 && series.when.front().date().daysTo(series.when.back().date()) <= 21;
    const double axis = top + rowHeight * static_cast<double>(series.levels.size());
    p.setPen(QColor{60, 60, 60});
    QDate last;
    for (size_t i = 0; i < n; i++) {
        const auto date = series.when[i].date();
        if (date != last && (i == 0 || daily || date.dayOfWeek() == 1)) {
            const double x = left + cell * (static_cast<double>(i) + 0.5);
            p.drawLine(QPointF{x, axis}, QPointF{x, axis + 5 * unit});
            p.drawText(QRectF{x - 50 * unit, axis + 6 * unit, 100 * unit, 18 * unit}, Qt::AlignHCenter | Qt::AlignTop, date.toString("MMM d"));
        }
        last = date;
    }
    // the color bar
    const double barTop = axis + 38 * unit, barHeight = 16 * unit, barLeft = left, barWidth = plotWidth * 0.6;
    for (int k = 0; k < 200; k++) {
        const double v = -reach + 2.0 * reach * k / 199.0;
        p.fillRect(QRectF{barLeft + barWidth * k / 200.0, barTop, barWidth / 200.0 + 0.6, barHeight}, windColor(v, reach));
    }
    p.setPen(QColor{60, 60, 60});
    for (const double v : {-reach, -reach / 2.0, 0.0, reach / 2.0, reach}) {
        const double x = barLeft + barWidth * (v + reach) / (2.0 * reach);
        p.drawText(QRectF{x - 30 * unit, barTop + barHeight + 2 * unit, 60 * unit, 16 * unit}, Qt::AlignHCenter | Qt::AlignTop, QString::number(v, 'f', 0));
    }
    p.end();
    return image;
}

bool VortexSection::collectWaves(const std::string& model, const std::vector<QDateTime>& when, int level, bool south, Waves& out, const std::function<bool(size_t, size_t)>& progress) {
    out = Waves{};
    out.when = when;
    out.level = level;
    out.south = south;
    const double nan = std::nan("");
    out.wave1.assign(when.size(), nan);
    out.wave2.assign(when.size(), nan);
    out.wind.assign(when.size(), nan);
    const double latitude = south ? -60.0 : 60.0;
    constexpr int points = 144;   // every 2.5 degrees
    bool any = false;
    for (size_t i = 0; i < when.size(); i++) {
        if (!progress(i, when.size())) {
            return false;
        }
        const auto sources = WorldRender::loadSources(std::to_string(level) + "_wnd_ht", {model}, when[i]);
        if (sources.empty()) {
            continue;
        }
        const auto sample = [latitude] (const GfsGrid::Grid& grid, double lon) {
            float v = grid.sample(lon, latitude);
            return std::isnan(v) ? grid.sample(lon + 360.0, latitude) : v;
        };
        if (!sources.front().windU.empty()) {
            double sum = 0.0;
            int n = 0;
            for (int k = 0; k < points; k++) {
                const float u = sample(sources.front().windU, -180.0 + 2.5 * k);
                if (!std::isnan(u)) {
                    sum += u;
                    n++;
                }
            }
            if (n > 0) {
                out.wind[i] = sum / n;
            }
        }
        if (sources.front().contours.empty()) {
            continue;
        }
        const auto& height = sources.front().contours.front().grid;
        std::vector<double> z;
        for (int k = 0; k < points; k++) {
            z.push_back(sample(height, -180.0 + 2.5 * k));
        }
        if (std::any_of(z.begin(), z.end(), [] (double v) { return std::isnan(v); })) {
            continue;
        }
        for (const int wave : {1, 2}) {
            double a = 0.0, b = 0.0;
            for (int k = 0; k < points; k++) {
                const double angle = 2.0 * M_PI * wave * k / points;
                a += z[k] * std::cos(angle);
                b += z[k] * std::sin(angle);
            }
            const double amplitude = 2.0 * std::hypot(a, b) / points;
            (wave == 1 ? out.wave1 : out.wave2)[i] = amplitude;
            any = true;
        }
    }
    return any;
}

QImage VortexSection::drawWaves(const Waves& waves, int width, const QString& title) {
    if (waves.when.size() < 2) {
        return {};
    }
    const double unit = width / 1200.0;
    const int left = static_cast<int>(80 * unit), right = static_cast<int>(80 * unit), top = static_cast<int>(80 * unit), bottom = static_cast<int>(60 * unit);
    const int plotHeight = static_cast<int>(380 * unit);
    QImage image{width, top + plotHeight + bottom, QImage::Format_ARGB32};
    image.fill(Qt::white);
    QPainter p{&image};
    p.setRenderHint(QPainter::Antialiasing);
    QFont font = p.font();
    font.setPixelSize(std::max(10, static_cast<int>(13 * unit)));
    QFont bold = font;
    bold.setBold(true);
    bold.setPixelSize(std::max(12, static_cast<int>(17 * unit)));
    p.setFont(bold);
    p.setPen(QColor{25, 25, 25});
    p.drawText(QRectF{left, 8 * unit, width - left - right, 26 * unit}, Qt::AlignLeft | Qt::AlignVCenter, title);
    p.setFont(font);
    p.setPen(QColor{80, 80, 80});
    p.drawText(QRectF{left, 36 * unit, width - left - right, 36 * unit}, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               QString{"%1 mb, the circle of 60°%2: amplitude of wave 1 (orange) and wave 2 (blue) of the height (m, left scale); the zonal-mean wind (gray, m/s, right scale)"}.arg(waves.level).arg(waves.south ? "S" : "N"));
    double heightTop = 10.0, windLow = 0.0, windHigh = 10.0;
    for (size_t i = 0; i < waves.when.size(); i++) {
        for (const double v : {waves.wave1[i], waves.wave2[i]}) {
            if (!std::isnan(v)) {
                heightTop = std::max(heightTop, v);
            }
        }
        if (!std::isnan(waves.wind[i])) {
            windLow = std::min(windLow, waves.wind[i]);
            windHigh = std::max(windHigh, waves.wind[i]);
        }
    }
    heightTop = std::ceil(heightTop / 200.0) * 200.0;   // (a top in hundreds, so the four ticks are round)
    windLow = std::floor(windLow / 10.0) * 10.0;
    windHigh = std::ceil(windHigh / 10.0) * 10.0;
    const double plotWidth = width - left - right;
    const size_t n = waves.when.size();
    const auto xAt = [&] (size_t i) { return left + plotWidth * static_cast<double>(i) / static_cast<double>(n - 1); };
    const auto yHeight = [&] (double v) { return top + plotHeight * (1.0 - v / heightTop); };
    const auto yWind = [&] (double v) { return top + plotHeight * (1.0 - (v - windLow) / (windHigh - windLow)); };
    p.setPen(QPen{QColor{225, 225, 225}, std::max(1.0, unit)});
    for (int k = 0; k <= 4; k++) {
        const double y = top + plotHeight * k / 4.0;
        p.drawLine(QPointF{left, y}, QPointF{left + plotWidth, y});
        p.setPen(QColor{60, 60, 60});
        p.drawText(QRectF{4 * unit, y - 9 * unit, left - 10 * unit, 18 * unit}, Qt::AlignRight | Qt::AlignVCenter, QString::number(heightTop * (1.0 - k / 4.0), 'f', 0));
        p.drawText(QRectF{left + plotWidth + 8 * unit, y - 9 * unit, right - 10 * unit, 18 * unit}, Qt::AlignLeft | Qt::AlignVCenter, QString::number(windLow + (windHigh - windLow) * (1.0 - k / 4.0), 'f', 0));
        p.setPen(QPen{QColor{225, 225, 225}, std::max(1.0, unit)});
    }
    if (windLow < 0.0) {   // the wind's zero
        p.setPen(QPen{QColor{150, 150, 150}, std::max(1.0, unit), Qt::DashLine});
        p.drawLine(QPointF{left, yWind(0.0)}, QPointF{left + plotWidth, yWind(0.0)});
    }
    const auto line = [&] (const std::vector<double>& values, const QColor& color, const std::function<double(double)>& y, double widthFactor) {
        p.setPen(QPen{color, std::max(1.0, widthFactor * unit), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        QPolygonF run;
        for (size_t i = 0; i < n; i++) {
            if (std::isnan(values[i])) {
                if (run.size() > 1) { p.drawPolyline(run); }
                run.clear();
            } else {
                run << QPointF{xAt(i), y(values[i])};
            }
        }
        if (run.size() > 1) { p.drawPolyline(run); }
    };
    line(waves.wind, QColor{140, 140, 140}, yWind, 1.5);
    line(waves.wave2, QColor{40, 100, 200}, yHeight, 2.5);
    line(waves.wave1, QColor{225, 120, 30}, yHeight, 2.5);
    p.setPen(QPen{QColor{120, 120, 120}, std::max(1.0, unit)});
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF{static_cast<double>(left), static_cast<double>(top), plotWidth, static_cast<double>(plotHeight)});
    p.setPen(QColor{60, 60, 60});
    const bool daily = waves.when.front().date().daysTo(waves.when.back().date()) <= 21;
    QDate last;
    for (size_t i = 0; i < n; i++) {
        const auto date = waves.when[i].date();
        if (date != last && (i == 0 || daily || date.dayOfWeek() == 1)) {
            p.drawLine(QPointF{xAt(i), top + plotHeight}, QPointF{xAt(i), top + plotHeight + 5 * unit});
            p.drawText(QRectF{xAt(i) - 50 * unit, top + plotHeight + 6 * unit, 100 * unit, 18 * unit}, Qt::AlignHCenter | Qt::AlignTop, date.toString("MMM d"));
        }
        last = date;
    }
    p.end();
    return image;
}
