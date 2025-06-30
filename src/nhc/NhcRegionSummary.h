// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NHCREGIONSUMMARY_H
#define NHCREGIONSUMMARY_H

#include <string>
#include <vector>
#include "nhc/NhcOceanEnum.h"

using std::string;
using std::vector;

class NhcRegionSummary {
public:
    explicit NhcRegionSummary(const NhcOceanEnum&);
    vector<string> urls;
    vector<string> titles;
};

#endif  // NHCREGIONSUMMARY_H
