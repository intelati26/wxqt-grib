// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WEATHERGRAPH_H
#define WEATHERGRAPH_H

#include <string>
#include <vector>
#include <QDateTime>
#include <QWidget>
#include "ui/Widget2.h"

using std::string;
using std::vector;

// The hourly forecast as a graph: temperature and dew point lines (left axis, F), chance of precipitation as bars
// (right axis, %), and a wind row (arrow pointing where the wind blows to, speed in mph) under the time axis.
// Shows the next `hoursShown` hours; days are marked at local midnight. Colours follow the light / dark theme.
class WeatherGraph : public QWidget, public Widget2 {
public:
    struct Point {
        QDateTime time;
        double temperature{0.0};           // F
        double dewpoint{-999.0};           // F, -999 when the forecast has none
        double precipitationChance{0.0};   // 0-100
        double windSpeed{0.0};             // mph (the higher number of a "5 to 10 mph" range)
        double windDirection{-1.0};        // degrees the wind blows from, -1 when unknown
        string condition;
    };

    explicit WeatherGraph(QWidget * parent = nullptr);
    ~WeatherGraph() override = default;
    QWidget * getView() override;
    void setData(const vector<Point>& points, const string& location);

    static constexpr int hoursShown{48};

private:
    void paintEvent(QPaintEvent *) override;
    vector<Point> points;
    string locationName;
    bool answered{false};   // setData was called (an empty answer means the forecast could not be had)
};

#endif  // WEATHERGRAPH_H
