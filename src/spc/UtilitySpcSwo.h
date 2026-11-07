// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSPCSWO_H
#define UTILITYSPCSWO_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// One of the SPC outlook page's risk tables: for each risk level the area, the population in it (2010 Census) and some larger cities in it
struct SpcRiskTable {
    string title;                    // "Day 2 Risk", "Day 1 Tornado Risk"
    vector<string> columns;          // the header cells
    vector<vector<string>> rows;     // risk, area, population, cities (one cell when it says "No Risk Areas Forecast")
};

class UtilitySpcSwo {
public:
    // the risk table files the outlook page of that day (1 to 3) loads, in the order it lists them (categorical first), e.g. "ac2_1730_SItable.html"
    static vector<string> riskTableFiles(const string& dayPageHtml);
    static SpcRiskTable parseRiskTable(const string& tableHtml);
    // the highest risk of a categorical table ("ENHANCED"), empty when it forecasts none
    static string highestRisk(const SpcRiskTable&);
    // the row of the highest risk (a category, or the largest percentage of a probability table): risk, area, population, cities; empty when there is none
    static vector<string> highestRow(const SpcRiskTable&);
    // the SPC's colour of a risk label ("MARGINAL", "TSTM", "15 %", ...), as #rrggbb; empty for one it has none for
    static string riskColor(const string& label);
    static string riskTableHtml(const SpcRiskTable&);
    static vector<string> getSwoStateUrl(string, int);
    static string getImageUrlsDays48(int);
    static vector<string> getImageUrls(int);
};

#endif  // UTILITYSPCSWO_H
