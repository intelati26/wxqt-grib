// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef FORECAST_H
#define FORECAST_H

#include <string>

using std::string;

class Forecast {
public:
    Forecast(const string&, const string&, const string&, const string&, const string&);
    string name;
    string temperature;
    string icon;
    string shortForecast;
    string detailedForecast;
};

#endif  // FORECAST_H
