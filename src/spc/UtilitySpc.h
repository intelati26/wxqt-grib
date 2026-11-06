// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSPC_H
#define UTILITYSPC_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilitySpc {
public:
    static vector<string> getTstormOutlookUrls();
    // the highest probability of a thunderstorm outlook's period, from its KML (the picture's address with .kml): percent 0 when no area is outlined
    struct TstormPeriod {
        int percent{0};
        string color;   // "#c5a392": the colour the outlook fills that probability with
        string valid;   // "06/20Z-07/00Z"
    };
    static TstormPeriod parseTstormKml(const string&);
    // when a day 1 to 3 convective outlook was issued and what it covers, from the outlook map service ("061556Z", as the SPC writes them)
    struct Issuance {
        string issued;
        string valid;
        string expire;
    };
    static string categoricalLayerUrl(int day);
    static Issuance parseIssuance(const string& json);
    static string issuedOfTstormKml(const string& kml);   // "061606Z": the KML says "Issue Time 20261106 202611061606Z"
};

#endif  // UTILITYSPC_H
