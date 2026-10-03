// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYHOURLY_H
#define UTILITYHOURLY_H

#include <string>
#include <unordered_map>
#include <vector>
#include "ui/WeatherGraph.h"

using std::string;
using std::unordered_map;
using std::vector;

class UtilityHourly {
public:
    static string get(int);
    // the same NWS hourly forecast the table shows, as numbers for WeatherGraph (blocks on the network: call it
    // off the UI thread); empty when the forecast cannot be had
    static vector<WeatherGraph::Point> getGraphData(int locationNumber);
    // the parsing half of getGraphData: NWS gridpoints/.../forecast/hourly JSON -> points
    static vector<WeatherGraph::Point> parseGraphData(const string& json);

private:
    static const unordered_map<string, string> hourlyAbbreviations;
    static string getFooter();
    static string getHourlyString(int);
    static string parse(const string&);
    static string shortenConditions(const string&);
};

#endif  // UTILITYHOURLY_H
