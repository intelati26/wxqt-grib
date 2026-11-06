// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYWPCRAINFALLOUTLOOK_H
#define UTILITYWPCRAINFALLOUTLOOK_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityWpcRainfallOutlook {
public:
    static const vector<string> codes;
    static const vector<string> urls;

    // what WPC's map service says of one day's outlook: the highest category, and when it was issued and for what period
    struct Day {
        string risk;     // "Marginal", "Slight", "Moderate" or "High"; empty: no area outlined
        string issued;   // "15:53Z"
        string valid;    // "16Z 11/06/26 - 12Z 11/07/26"
    };
    static string layerUrl(int day);   // day 1 to 5
    static Day parseLayer(const string& json);
    static string riskColor(const string& risk);   // the colours of WPC's own map
};

#endif  // UTILITYWPCRAINFALLOUTLOOK_H
