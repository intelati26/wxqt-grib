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
#include "misc/UtilityOpenMeteoPressure.h"
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
        string label;          // the time as text, for the axis
        long long utc{0};      // the time, seconds since 1970 (0: not known)
    };

    vector<DataPoint> dataPoints;
    vector<UtilityOpenMeteoPressure::Sample> pressure;
    string locationName;
    QFont axisFont;
    QFont labelFont;

public:
    WeatherGraph(QWidget * parent = nullptr);
    void setData(const vector<string>& times, const vector<double>& temperatures,
                 const vector<double>& windSpeeds, const vector<double>& windDirections,
                 const vector<string>& conditions, const string& location);
    
    // the time of each point (seconds since 1970), and the sea level pressure samples (every third hour) drawn as a green line against its own axis; empty: none
    void setTimes(const vector<long long>& utc);
    void setPressure(const vector<UtilityOpenMeteoPressure::Sample>& samples);
    const vector<DataPoint>& getDataPoints() const { return dataPoints; }

private:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
};

#endif // WEATHERGRAPH_H
