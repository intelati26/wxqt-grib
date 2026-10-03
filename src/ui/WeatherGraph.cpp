// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file for the official project for license.
// *****************************************************************************

#include "ui/WeatherGraph.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QDateTime>
#include <QResizeEvent>
#include "objects/WString.h"

WeatherGraph::WeatherGraph(QWidget * parent) : QWidget(parent) {
    setMinimumSize(600, 400);
    axisFont.setPixelSize(10);
    labelFont.setPixelSize(11);
}

QWidget * WeatherGraph::getView() {
    return this;
}

void WeatherGraph::setData(const vector<string>& times, const vector<double>& temperatures,
                          const vector<double>& windSpeeds, const vector<double>& windDirections,
                          const vector<string>& conditions, const string& location) {
    locationName = location;
    dataPoints.clear();

    if (times.size() != temperatures.size() || times.size() != windSpeeds.size() ||
        times.size() != windDirections.size() || times.size() != conditions.size()) {
        return; // Data mismatch, clear
    }

    for (size_t i = 0; i < times.size(); ++i) {
        DataPoint point;
        point.time = std::stod(times[i]);
        point.temperature = temperatures[i];
        point.windSpeed = windSpeeds[i];
        point.windDirection = windDirections[i];
        point.condition = conditions[i];
        dataPoints.push_back(point);
    }

    update();
}

void WeatherGraph::paintEvent(QPaintEvent *) {
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor{250, 250, 250});

    if (dataPoints.empty()) {
        painter.setPen(QColor{100, 100, 100});
        painter.drawText(rect(), Qt::AlignCenter, "No data available");
        return;
    }

    // Find data ranges
    double timeLo = dataPoints[0].time;
    double timeHi = dataPoints[0].time;
    double tempLo = dataPoints[0].temperature;
    double tempHi = dataPoints[0].temperature;
    double windLo = dataPoints[0].windSpeed;
    double windHi = dataPoints[0].windSpeed;
    double windDirLo = dataPoints[0].windDirection;
    double windDirHi = dataPoints[0].windDirection;

    for (const auto& point : dataPoints) {
        timeLo = std::min(timeLo, point.time);
        timeHi = std::max(timeHi, point.time);
        tempLo = std::min(tempLo, point.temperature);
        tempHi = std::max(tempHi, point.temperature);
        windLo = std::min(windLo, point.windSpeed);
        windHi = std::max(windHi, point.windSpeed);
        windDirLo = std::min(windDirLo, point.windDirection);
        windDirHi = std::max(windDirHi, point.windDirection);
    }

    // Add some padding to ranges
    if (timeHi - timeLo < 1.0) {
        timeLo -= 0.5;
        timeHi += 0.5;
    }
    if (tempHi - tempLo < 2.0) {
        tempLo -= 1.0;
        tempHi += 1.0;
    }
    if (windHi - windLo < 5.0) {
        windLo -= 2.5;
        windHi += 2.5;
    }
    if (windDirHi - windDirLo < 10.0) {
        windDirLo -= 5.0;
        windDirHi += 5.0;
    }

    // Create plot area
    QRect plot{60, 40, width() - 120, height() - 80};

    // Draw grid and axes
    painter.setPen(QColor{225, 225, 225});
    painter.drawRect(plot);

    painter.setPen(QColor{60, 60, 60});
    painter.setFont(axisFont);

    // Time axis (bottom)
    int timeSteps = std::max(6, static_cast<int>(dataPoints.size()));
    for (int i = 0; i <= timeSteps; ++i) {
        double time = timeLo + (timeHi - timeLo) * i / timeSteps;
        int x = plot.left() + plot.width() * (time - timeLo) / (timeHi - timeLo);
        painter.drawLine(x, plot.bottom(), x, plot.bottom() + 5);
        painter.drawText(x - 15, plot.bottom() + 15, QString::number(time, 'f', 0));
    }

    // Temperature axis (left)
    int tempSteps = 5;
    for (int i = 0; i <= tempSteps; ++i) {
        double temp = tempLo + (tempHi - tempLo) * i / tempSteps;
        int y = plot.bottom() - plot.height() * i / tempSteps;
        painter.drawLine(plot.left() - 5, y, plot.left(), y);
        painter.drawText(8, y + 4, QString::number(temp, 'f', 1));
    }

    // Wind speed axis (right)
    int windSteps = 5;
    for (int i = 0; i <= windSteps; ++i) {
        double wind = windLo + (windHi - windLo) * i / windSteps;
        int y = plot.bottom() - plot.height() * i / windSteps;
        painter.drawLine(plot.right() + 2, y, plot.right() - 2, y);
        painter.drawText(plot.right() - 8, y + 4, QString::number(wind, 'f', 1));
    }

    // Wind direction axis (top)
    int dirSteps = 8;
    for (int i = 0; i <= dirSteps; ++i) {
        double dir = windDirLo + (windDirHi - windDirLo) * i / dirSteps;
        int x = plot.left() + plot.width() * (dir - windDirLo) / (windDirHi - windDirLo);
        painter.drawLine(x, plot.top() - 5, x, plot.top() + 5);
        painter.drawText(x - 15, plot.top() - 10, QString::number(dir, 'f', 0));
    }

    // Draw temperature line (red)
    painter.setPen(QPen{QColor{220, 50, 50}, 3.0});
    QPointF prev;
    bool havePrev = false;
    for (const auto& point : dataPoints) {
        int x = plot.left() + plot.width() * (point.time - timeLo) / (timeHi - timeLo);
        int y = plot.bottom() - plot.height() * (point.temperature - tempLo) / (tempHi - tempLo);
        if (havePrev) {
            painter.drawLine(prev, QPointF(x, y));
        }
        prev = QPointF(x, y);
        havePrev = true;
    }

    // Draw wind speed line (blue)
    painter.setPen(QPen{QColor{50, 50, 220}, 2.0});
    havePrev = false;
    for (const auto& point : dataPoints) {
        int x = plot.left() + plot.width() * (point.time - timeLo) / (timeHi - timeLo);
        int y = plot.bottom() - plot.height() * (point.windSpeed - windLo) / (windHi - windLo);
        if (havePrev) {
            painter.drawLine(prev, QPointF(x, y));
        }
        prev = QPointF(x, y);
        havePrev = true;
    }

    // Draw legend
    painter.setPen(QColor{40, 40, 40});
    painter.setFont(labelFont);
    painter.drawText(10, plot.bottom() + 25, QString::fromStdString("Location: " + locationName));
    painter.drawText(10, plot.bottom() + 45, "Temperature: °F (red line)");
    painter.drawText(10, plot.bottom() + 65, "Wind Speed: mph (blue line)");

    // Add condition labels for some points
    painter.setPen(QColor{80, 80, 80});
    for (size_t i = 0; i < dataPoints.size(); i += 4) {
        const auto& point = dataPoints[i];
        int x = plot.left() + plot.width() * (point.time - timeLo) / (timeHi - timeLo);
        int y = plot.top() - 20;
        painter.drawText(x - 20, y, QString::fromStdString(point.condition));
    }
}

void WeatherGraph::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    update();
}
