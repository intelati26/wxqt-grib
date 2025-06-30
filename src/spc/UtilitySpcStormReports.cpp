// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpcStormReports.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"

vector<StormReport> UtilitySpcStormReports::process(const vector<string>& lines) {
    vector<StormReport> stormReports;
    for (const auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        string lat;
        string lon;
        string state;
        string time;
        string address;
        string damageReport;
        string magnitude;
        string city;
        string output;
        string damageHeader;
        if (WString::contains(line, ",F_Scale,")) {
            damageHeader = "Tornado Reports";
        } else if (WString::contains(line, ",Speed,")) {
            damageHeader = "Wind Reports";
        } else if (WString::contains(line, ",Size,")) {
            damageHeader = "Hail Reports";
        } else {
            const auto lineChunks = WString::split(line, ",");
            if (lineChunks.size() > 7) {
                output += lineChunks[0];
                output += " ";
                output += lineChunks[1];
                output += " ";
                output += lineChunks[2];
                output += " ";
                output += lineChunks[3];
                output += " ";
                output += lineChunks[4];
                output += " ";
                output += lineChunks[5];
                output += " ";
                output += lineChunks[6];
                output += GlobalVariables::newline;
                output += lineChunks[7];
                time = lineChunks[0];
                magnitude = lineChunks[1];
                address = lineChunks[2];
                city = lineChunks[3];
                state = lineChunks[4];
                lat = lineChunks[5];
                lon = lineChunks[6];
                damageReport = lineChunks[7];
            }
        }
        stormReports.emplace_back(output, lat, lon, time, magnitude, address, city, state, damageReport, damageHeader);
    }
    return stormReports;
}
