// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COLORPALETTELINE_H
#define COLORPALETTELINE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class ColorPaletteLine {
public:
    explicit ColorPaletteLine(const vector<string>&);
    ColorPaletteLine(int, const string&, const string&, const string&);
    static ColorPaletteLine fourBit(const vector<string>&);
    vector<double> asVector() const;
    int dbz;
    int red;
    int green;
    int blue;
};

#endif  // COLORPALETTELINE_H
