// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYCOLORPALETTE_H
#define UTILITYCOLORPALETTE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityColorPalette {
public:
    static vector<string> getColorMapStringFromDisk(int, const string&);
};

#endif  // UTILITYCOLORPALETTE_H
