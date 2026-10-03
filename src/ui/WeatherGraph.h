// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WEATHERGRAPH_H
#define WEATHERGRAPH_H

#include <string>
#include <vector>
#include <QWidget>
#include "ui/Window.h"
#include "ui/Widget2.h"

using std::string;
using std::vector;

class WeatherGraph : public QWidget, public Widget2 {
public:
    QWidget * getView() override;
    virtual ~WeatherGraph() override {};

private:
    struct DataPoint {
        double time;           // hour value
        double temperature;    // temperature in degrees
        double windSpeed;      // wind speed
        double windDirection;  // wind direction
        string condition;      // weather condition
    };

    vector<DataPoint> dataPoints;
    string locationName;
    QFont axisFont;
    QFont labelFont;

public:
    WeatherGraph(QWidget * parent = nullptr);
    void setData(const vector<string>& times, const vector<double>& temperatures,
                 const vector<double>& windSpeeds, const vector<double>& windDirections,
                 const vector<string>& conditions, const string& location);
    
    const vector<DataPoint>& getDataPoints() const { return dataPoints; }

private:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
};

#endif // WEATHERGRAPH_H
