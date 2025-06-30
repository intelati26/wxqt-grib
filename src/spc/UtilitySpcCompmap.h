// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCUTILITYSPCCOMPMAP_H
#define SPCUTILITYSPCCOMPMAP_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilitySpcCompmap {
public:
    static string getImage(const string&);
    static const vector<string> labels;
    static const vector<string> urlIndices;
};

#endif  // SPCUTILITYSPCCOMPMAP_H
