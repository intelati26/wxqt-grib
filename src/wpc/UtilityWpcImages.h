// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYWPCIMAGES_H
#define UTILITYWPCIMAGES_H

#include <string>
#include <vector>
#include "ui/MenuTitle.h"

using std::string;
using std::vector;

class UtilityWpcImages {
public:
    static vector<MenuTitle> titles;
    static const vector<string> urls;
    static const vector<string> labels;
    // the picture's address (the forecast-chart addresses are completed with the file name)
    static string imageUrl(int index);
    // the entries of the menu group `index` is in, in listed order: what a loop steps through (a run of forecast days / hours)
    static vector<int> seriesOf(int index);
    // a short file-name-safe name for the group
    static string seriesName(int index);
};

#endif  // UTILITYWPCIMAGES_H
