// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELS_H
#define UTILITYMODELS_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityModels {
public:
    static string convertTimeRuntoTimeString(const string&, const string&);
    static vector<string> updateTime(const string&, const string&, const vector<string>&, const string&);
    static const string urlSeperator;
};

#endif  // UTILITYMODELS_H
