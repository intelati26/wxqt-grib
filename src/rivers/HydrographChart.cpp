// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "rivers/HydrographChart.h"
#include "ui/ChartExport.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

namespace {
    // a rounded step (1, 2, 5 x 10^n) for about `count` divisions of `span`
    double niceStep(double span, int count) {
        if (span <= 0.0) {
            return 1.0;
        }
        const double raw = span / count;
        const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
        const double fraction = raw / magnitude;
        const double step = fraction <= 1.0 ? 1.0 : fraction <= 2.0 ? 2.0 : fraction <= 5.0 ? 5.0 : 10.0;
        return step * magnitude;
    }

    QString timeText(long long time, bool withHour) {
        return QDateTime::fromSecsSinceEpoch(time, Qt::UTC).toString(withHour ? "yyyy-MM-dd HH:mm'Z'" : "MM-dd");
    }
}

HydrographChart::HydrographChart(QWidget * parent)
    : QWidget{parent}
{
    setMouseTracking(true);
    setMinimumHeight(280);
    ChartExport::install(this, "River hydrograph");
}

void HydrographChart::setData(const std::vector<Line>& newLines, const std::vector<Level>& newLevels, const QString& newUnit, long long nowTime) {
    lines = newLines;
    unit = newUnit;
    now = nowTime;
    tMin = 0;
    tMax = 0;
    bool first = true;
    double lo = 0.0;
    double hi = 0.0;
    for (const auto& line : lines) {
        for (const auto& point : line.points) {
            if (first) {
                tMin = tMax = point.time;
                lo = hi = point.value;
                first = false;
            } else {
                tMin = std::min(tMin, point.time);
                tMax = std::max(tMax, point.time);
                lo = std::min(lo, point.value);
                hi = std::max(hi, point.value);
            }
        }
    }
    if (tMax <= tMin) {
        tMax = tMin + 3600;
    }
    // the flood levels that are near the data (a level far above it would squash the river into a line)
    const double span = std::max(hi - lo, 0.5);
    levels.clear();
    for (const auto& level : newLevels) {
        if (UtilityRivers::has(level.value) && level.value <= hi + 0.4 * span && level.value >= lo - span) {
            levels.push_back(level);
            hi = std::max(hi, level.value);
        }
    }
    const double pad = std::max(hi - lo, 0.5) * 0.06;
    vMin = lo - pad;
    vMax = hi + pad;
    update();
}

QRectF HydrographChart::plotArea() const {
    return QRectF{64.0, 14.0, width() - 64.0 - 14.0, height() - 14.0 - 34.0};
}

double HydrographChart::xOf(long long time) const {
    const auto area = plotArea();
    return area.left() + area.width() * static_cast<double>(time - tMin) / static_cast<double>(tMax - tMin);
}

double HydrographChart::yOf(double value) const {
    const auto area = plotArea();
    return area.bottom() - area.height() * (value - vMin) / (vMax - vMin);
}

void HydrographChart::paintEvent(QPaintEvent *) {
    ChartPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto& pal = palette();
    painter.fillRect(rect(), pal.color(QPalette::Base));
    const auto text = pal.color(QPalette::Text);
    const auto grid = pal.color(QPalette::Mid);
    const auto area = plotArea();
    QFont small{painter.font()};
    small.setPixelSize(std::max(10, QFontInfo{small}.pixelSize() - 2));
    painter.setFont(small);
    if (lines.empty() || std::all_of(lines.begin(), lines.end(), [] (const Line& l) { return l.points.empty(); })) {
        painter.setPen(text);
        painter.drawText(rect(), Qt::AlignCenter, "No readings to show");
        return;
    }
    // the horizontal grid and the value labels
    const double step = niceStep(vMax - vMin, 6);
    painter.setPen(QPen{grid, 1.0});
    for (double v = std::ceil(vMin / step) * step; v <= vMax; v += step) {
        const double y = yOf(v);
        painter.setPen(QPen{grid, 0.6, Qt::DotLine});
        painter.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
        painter.setPen(text);
        painter.drawText(QRectF{0.0, y - 8.0, area.left() - 6.0, 16.0}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'g', 6));
    }
    // the time axis: a label at each UTC midnight (every few days for a long span)
    const double days = static_cast<double>(tMax - tMin) / 86400.0;
    const int everyDays = days <= 8 ? 1 : days <= 18 ? 2 : days <= 40 ? 5 : 10;
    const long long firstMidnight = (tMin / 86400 + 1) * 86400;
    for (long long t = firstMidnight; t <= tMax; t += 86400) {
        if ((t / 86400) % everyDays != 0) {
            continue;
        }
        const double x = xOf(t);
        painter.setPen(QPen{grid, 0.6, Qt::DotLine});
        painter.drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
        painter.setPen(text);
        painter.drawText(QRectF{x - 40.0, area.bottom() + 4.0, 80.0, 16.0}, Qt::AlignCenter, timeText(t, false));
    }
    painter.drawText(QRectF{area.left(), area.bottom() + 18.0, area.width(), 14.0}, Qt::AlignRight, "UTC");
    painter.drawText(QRectF{4.0, 0.0, 200.0, 14.0}, Qt::AlignLeft | Qt::AlignTop, unit);
    painter.setPen(QPen{text, 1.0});
    painter.drawRect(area);

    painter.save();
    painter.setClipRect(area.adjusted(-1, -1, 1, 1));
    // the flood levels
    for (const auto& level : levels) {
        const double y = yOf(level.value);
        painter.setPen(QPen{level.color, 1.6, Qt::DashLine});
        painter.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
        painter.setPen(level.color);
        painter.drawText(QRectF{area.left(), y - 15.0, area.width() - 6.0, 14.0}, Qt::AlignRight | Qt::AlignBottom, level.label);   // at the right end, clear of the key
    }
    // now
    if (now > tMin && now < tMax) {
        painter.setPen(QPen{text, 1.0, Qt::DashDotLine});
        painter.drawLine(QPointF{xOf(now), area.top()}, QPointF{xOf(now), area.bottom()});
    }
    // the lines
    for (const auto& line : lines) {
        if (line.points.empty()) {
            continue;
        }
        QPainterPath path;
        bool started = false;
        long long previous = 0;
        for (const auto& point : line.points) {
            const QPointF at{xOf(point.time), yOf(point.value)};
            // a long gap in the readings is a gap in the line
            if (!started || point.time - previous > 6 * 3600 + 1) {
                path.moveTo(at);
                started = true;
            } else {
                path.lineTo(at);
            }
            previous = point.time;
        }
        painter.setPen(QPen{line.color, line.width, line.style, Qt::RoundCap, Qt::RoundJoin});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
    painter.restore();

    // the key
    double keyX = area.left() + 8.0;
    const double keyY = area.top() + 8.0;
    for (const auto& line : lines) {
        if (line.points.empty()) {
            continue;
        }
        painter.setPen(QPen{line.color, line.width, line.style});
        painter.drawLine(QPointF{keyX, keyY + 14.0}, QPointF{keyX + 22.0, keyY + 14.0});
        painter.setPen(text);
        const auto label = line.name;
        const double labelWidth = QFontMetricsF{small}.horizontalAdvance(label);
        painter.drawText(QPointF{keyX + 27.0, keyY + 18.0}, label);
        keyX += 27.0 + labelWidth + 16.0;
    }

    // the readout under the pointer
    if (hover && area.contains(pointer)) {
        const auto time = tMin + static_cast<long long>((pointer.x() - area.left()) / area.width() * static_cast<double>(tMax - tMin));
        painter.setPen(QPen{text, 1.0});
        painter.drawLine(QPointF{pointer.x(), area.top()}, QPointF{pointer.x(), area.bottom()});
        QStringList rows{timeText(time, true)};
        for (const auto& line : lines) {
            // the nearest reading, if one is within a few hours
            const UtilityRivers::Point * best = nullptr;
            for (const auto& point : line.points) {
                if (best == nullptr || std::llabs(point.time - time) < std::llabs(best->time - time)) {
                    best = &point;
                }
            }
            if (best != nullptr && std::llabs(best->time - time) <= 4 * 3600) {
                rows << line.name + ": " + QString::number(best->value, 'g', 6);
                painter.setBrush(line.color);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF{xOf(best->time), yOf(best->value)}, 3.5, 3.5);
            }
        }
        painter.setPen(text);
        const QFontMetricsF metrics{small};
        double widest = 0.0;
        for (const auto& row : rows) {
            widest = std::max(widest, metrics.horizontalAdvance(row));
        }
        const double boxWidth = widest + 14.0;
        const double boxHeight = rows.size() * 15.0 + 8.0;
        double bx = pointer.x() + 12.0;
        if (bx + boxWidth > area.right()) {
            bx = pointer.x() - 12.0 - boxWidth;
        }
        const double by = std::clamp(pointer.y() - boxHeight / 2.0, area.top(), area.bottom() - boxHeight);
        painter.setBrush(pal.color(QPalette::ToolTipBase));
        painter.setPen(QPen{grid, 1.0});
        painter.drawRoundedRect(QRectF{bx, by, boxWidth, boxHeight}, 3.0, 3.0);
        painter.setPen(pal.color(QPalette::ToolTipText));
        for (int i = 0; i < rows.size(); i += 1) {
            painter.drawText(QPointF{bx + 7.0, by + 15.0 + i * 15.0}, rows[i]);
        }
    }
}

void HydrographChart::mouseMoveEvent(QMouseEvent * event) {
    pointer = event->position();
    hover = true;
    update();
}

void HydrographChart::leaveEvent(QEvent *) {
    hover = false;
    update();
}
