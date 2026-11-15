// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/VortexSection.h"
#include <algorithm>
#include <cmath>
#include <QFont>
#include <QPainter>
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
    // the dates: a tick at the first of each week of the range
    const double axis = top + rowHeight * static_cast<double>(series.levels.size());
    p.setPen(QColor{60, 60, 60});
    QDate last;
    for (size_t i = 0; i < n; i++) {
        const auto date = series.when[i].date();
        if (date != last && (i == 0 || date.dayOfWeek() == 1)) {
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
