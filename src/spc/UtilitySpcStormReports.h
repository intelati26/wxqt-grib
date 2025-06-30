// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSPCSTORMREPORTS_H
#define UTILITYSPCSTORMREPORTS_H

#include <string>
#include <vector>
#include "spc/StormReport.h"

using std::string;
using std::vector;

class UtilitySpcStormReports {
public:
    static vector<StormReport> process(const vector<string>&);
};

#endif  // UTILITYSPCSTORMREPORTS_H
