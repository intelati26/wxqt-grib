// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/WeatherGraph.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>

namespace {
    // a step of 2, 5 or 10 (times a power of ten) that gives about `count` ticks over the range
    double niceStep(double range, int count) {
        const auto raw = range / std::max(1, count);
        const auto power = std::pow(10.0, std::floor(std::log10(raw)));
        for (const auto factor : {1.0, 2.0, 5.0, 10.0}) {
            if (raw <= factor * power) {
                return factor * power;
            }
        }
        return 10.0 * power;
    }

    QString hourLabel(const QDateTime& time) {
        const auto hour = time.time().hour();
        if (hour == 0) {
            return "12a";
        }
        if (hour == 12) {
            return "12p";
        }
        return QString::number(hour % 12) + (hour < 12 ? "a" : "p");
    }
}

WeatherGraph::WeatherGraph(QWidget * parent)
    : QWidget{parent}
{
    setMinimumSize(320, 230);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QWidget * WeatherGraph::getView() {
    return this;
}

void WeatherGraph::setData(const vector<Point>& newPoints, const string& location) {
    // the forecast's own UTC offset is kept, so the hours shown are the location's local hours
    points.assign(newPoints.begin(), newPoints.begin() + std::min(newPoints.size(), static_cast<size_t>(hoursShown)));
    locationName = location;
    answered = true;
    if (!points.empty()) {
        setToolTip(QString::fromStdString("Hourly forecast for " + locationName + " - next " +
                                          std::to_string(points.size()) + " hours"));
    }
    update();
}

void WeatherGraph::paintEvent(QPaintEvent *) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    const auto text = palette().color(QPalette::WindowText);
    auto grid = text;
    grid.setAlpha(45);
    const QColor temperatureColor{220, 60, 50};
    const QColor dewpointColor{40, 160, 70};
    QColor rainColor{60, 130, 220};
    rainColor.setAlpha(110);

    if (points.size() < 2) {
        painter.setPen(text);
        painter.drawText(rect(), Qt::AlignCenter, answered ? "Hourly forecast not available (offline or the NWS did not answer)"
                                                            : "Hourly graph: waiting for the forecast...");
        return;
    }

    const auto metrics = painter.fontMetrics();
    const int lineHeight = metrics.height();
    const int windRow = lineHeight * 2 + 6;   // arrows, then speeds
    const QRect plot{metrics.horizontalAdvance("100") + 10, lineHeight * 3 / 2 + 6,
                     width() - metrics.horizontalAdvance("100") * 2 - 22,
                     height() - lineHeight * 7 / 2 - windRow - 10};
    if (plot.width() < 50 || plot.height() < 40) {
        return;
    }

    // temperature axis covers both lines, padded and snapped to round numbers
    double low = points.front().temperature;
    double high = low;
    for (const auto& point : points) {
        low = std::min(low, point.temperature);
        high = std::max(high, point.temperature);
        if (point.dewpoint > -900.0) {
            low = std::min(low, point.dewpoint);
            high = std::max(high, point.dewpoint);
        }
    }
    const auto step = niceStep(std::max(4.0, high - low), 4);
    low = std::floor((low - 1.0) / step) * step;
    high = std::ceil((high + 1.0) / step) * step;

    const auto count = static_cast<int>(points.size());
    auto xFor = [&] (int index) { return plot.left() + plot.width() * index / static_cast<double>(count - 1); };
    auto yForTemperature = [&] (double value) { return plot.bottom() - plot.height() * (value - low) / (high - low); };
    auto yForPercent = [&] (double value) { return plot.bottom() - plot.height() * value / 100.0; };

    // key along the top
    {
        int x = 4;
        const auto drawKey = [&] (const QString& label, const QColor& color) {
            painter.setPen(color);
            painter.drawText(x, lineHeight - 3, label);
            x += metrics.horizontalAdvance(label) + 14;
        };
        drawKey("Temp F", temperatureColor);
        drawKey("Dew point F", dewpointColor);
        auto rainKey = rainColor;
        rainKey.setAlpha(255);
        drawKey("Precip chance %", rainKey);
        painter.setPen(text);
        drawKey("Wind mph", text);
    }

    // horizontal grid + temperature labels (left), percent labels (right)
    for (double value = low; value <= high + 0.01; value += step) {
        const auto y = yForTemperature(value);
        painter.setPen(grid);
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(text);
        painter.drawText(QRectF(0, y - lineHeight / 2.0, plot.left() - 6, lineHeight), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(value, 'f', 0));
    }
    for (int percent = 0; percent <= 100; percent += 50) {
        const auto y = yForPercent(percent);
        painter.setPen(text);
        painter.drawText(QRectF(plot.right() + 6, y - lineHeight / 2.0, 60, lineHeight), Qt::AlignLeft | Qt::AlignVCenter,
                         QString::number(percent) + "%");
    }

    // precipitation chance bars
    const auto barWidth = std::max(1.0, plot.width() / static_cast<double>(count) * 0.7);
    for (int index = 0; index < count; index += 1) {
        const auto chance = points[static_cast<size_t>(index)].precipitationChance;
        if (chance > 0.0) {
            const auto x = xFor(index);
            painter.fillRect(QRectF(x - barWidth / 2.0, yForPercent(chance), barWidth, plot.bottom() - yForPercent(chance)), rainColor);
        }
    }

    // day boundaries and hour labels
    const int labelEvery = plot.width() / count >= 14 ? 3 : 6;
    for (int index = 0; index < count; index += 1) {
        const auto& time = points[static_cast<size_t>(index)].time;
        const auto x = xFor(index);
        if (time.time().hour() == 0) {
            painter.setPen(QPen{text, 1.0, Qt::DashLine});
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            painter.setPen(text);
            painter.drawText(QPointF(x + 3, plot.top() + lineHeight - 2), time.toString("ddd"));
        }
        if (time.time().hour() % labelEvery == 0) {
            painter.setPen(text);
            painter.drawText(QRectF(x - 20, plot.bottom() + 2, 40, lineHeight), Qt::AlignCenter, hourLabel(time));
        }
    }

    // temperature and dew point lines
    const auto drawLine = [&] (const QColor& color, auto valueOf) {
        QPainterPath path;
        bool started = false;
        for (int index = 0; index < count; index += 1) {
            const auto value = valueOf(points[static_cast<size_t>(index)]);
            if (value < -900.0) {
                started = false;
                continue;
            }
            const QPointF at{xFor(index), yForTemperature(value)};
            if (started) {
                path.lineTo(at);
            } else {
                path.moveTo(at);
                started = true;
            }
        }
        painter.setPen(QPen{color, 2.2});
        painter.drawPath(path);
    };
    drawLine(dewpointColor, [] (const Point& point) { return point.dewpoint; });
    drawLine(temperatureColor, [] (const Point& point) { return point.temperature; });

    // the highest and lowest temperature, labelled
    const auto [lowest, highest] = std::minmax_element(points.begin(), points.end(),
        [] (const Point& a, const Point& b) { return a.temperature < b.temperature; });
    painter.setPen(temperatureColor);
    for (const auto it : {lowest, highest}) {
        const auto index = static_cast<int>(it - points.begin());
        const auto y = yForTemperature(it->temperature);
        painter.drawText(QRectF(xFor(index) - 20, it == highest ? y - lineHeight - 2 : y + 2, 40, lineHeight),
                         Qt::AlignCenter, QString::number(it->temperature, 'f', 0));
    }

    // wind row: an arrow pointing where the wind blows to, the speed under it
    const auto arrowY = plot.bottom() + lineHeight + 4 + lineHeight / 2.0;
    const auto speedY = arrowY + lineHeight / 2.0 + 2;
    for (int index = 0; index < count; index += labelEvery) {
        const auto& point = points[static_cast<size_t>(index)];
        const auto x = xFor(index);
        painter.setPen(text);
        if (point.windDirection >= 0.0 && point.windSpeed > 0.0) {
            painter.save();
            painter.translate(x, arrowY);
            painter.rotate(point.windDirection + 180.0);   // from-direction -> pointing downwind; 0 deg = up
            const double size = lineHeight * 0.4;
            painter.setPen(QPen{text, 1.4});
            painter.drawLine(QPointF(0, size), QPointF(0, -size));
            painter.drawLine(QPointF(0, -size), QPointF(-size * 0.5, -size * 0.4));
            painter.drawLine(QPointF(0, -size), QPointF(size * 0.5, -size * 0.4));
            painter.restore();
        }
        painter.drawText(QRectF(x - 20, speedY, 40, lineHeight), Qt::AlignCenter,
                         point.windSpeed > 0.0 ? QString::number(point.windSpeed, 'f', 0) : "calm");
    }
}
