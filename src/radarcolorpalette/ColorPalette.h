// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COLORPALETTE_H
#define COLORPALETTE_H

#include <string>
#include <unordered_map>
#include <vector>
#include "objects/MemoryBuffer.h"
#include "radarcolorpalette/ColorPaletteLine.h"

using std::string;
using std::unordered_map;
using std::vector;

class ColorPalette {
public:
    explicit ColorPalette(int);
    void putInt(const vector<double>&);
    void putBytesFromLine(const ColorPaletteLine&);
    void initialize();
    void position(int);
    static void loadColorMap(int);
    static void refreshPref();
    MemoryBuffer redValues;
    MemoryBuffer greenValues;
    MemoryBuffer blueValues;
    static unordered_map<int, string> radarColorPalette;
    static unordered_map<int, ColorPalette *> colorMap;

private:
    void setupBuffers(int);
    static void generate(int, const string&);
    static void generate4bitGeneric(int);
    static void loadColorMap165();
    int colorMapCode;
};

#endif  // COLORPALETTE_H
