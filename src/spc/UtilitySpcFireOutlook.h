// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSPCFIREOUTLOOK_H
#define UTILITYSPCFIREOUTLOOK_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilitySpcFireOutlook {
public:
    // What the SPC's fire weather page says above each outlook picture (day 1, day 2, days 3-8, in that order; fewer when the page has fewer): who issued it, when, and the risk
    struct Issuance {
        string forecaster, issued, valid, risk;
        string line() const;   // "Forecaster WILLIAMS   Issued 111618Z   Valid 111700Z - 121200Z   Risk: Elevated"
    };
    static vector<Issuance> issuances(const string& pageHtml);
    static const vector<string> products;
    static const vector<string> urls;
};

#endif  // UTILITYSPCFIREOUTLOOK_H
