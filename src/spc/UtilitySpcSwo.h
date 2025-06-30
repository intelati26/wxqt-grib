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

class UtilitySpcSwo {
public:
    static vector<string> getSwoStateUrl(string, int);
    static string getImageUrlsDays48(int);
    static vector<string> getImageUrls(int);
};

#endif  // UTILITYSPCSWO_H
