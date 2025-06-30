// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ColorPaletteLine.h"
#include "util/To.h"

ColorPaletteLine::ColorPaletteLine(const vector<string>& items)
    : dbz{To::Int(items[1])}
    , red{To::Int(items[2])}
    , green{To::Int(items[3])}
    , blue{To::Int(items[4])}
{}

ColorPaletteLine::ColorPaletteLine(int dbz, const string& r, const string& g, const string& b)
    : dbz{dbz}
    , red{To::Int(r)}
    , green{To::Int(g)}
    , blue{To::Int(b)}
{}

ColorPaletteLine ColorPaletteLine::fourBit(const vector<string>& items) {
    return {0, items[0], items[1], items[2]};
}

vector<double> ColorPaletteLine::asVector() const {
    return {red / 255.0, green / 255.0, blue / 255.0};
}
