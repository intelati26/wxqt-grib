// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ColorPalettes.h"
#include "radarcolorpalette/ColorPalette.h"

void ColorPalettes::initialize() {
    ColorPalette::refreshPref();
    ColorPalette::colorMap[94] = new ColorPalette{94};
    ColorPalette::colorMap[94]->initialize();
    ColorPalette::colorMap[99] = new ColorPalette{99};
    ColorPalette::colorMap[99]->initialize();
    ColorPalette::colorMap[153] = ColorPalette::colorMap[94];
    ColorPalette::colorMap[180] = ColorPalette::colorMap[94];
    ColorPalette::colorMap[186] = ColorPalette::colorMap[94];
    ColorPalette::colorMap[154] = ColorPalette::colorMap[99];
    ColorPalette::colorMap[182] = ColorPalette::colorMap[99];
    ColorPalette::colorMap[172] = new ColorPalette{172};
    ColorPalette::colorMap[172]->initialize();
    ColorPalette::colorMap[170] = ColorPalette::colorMap[172];
    for (auto it : {19, 30, 56, 134, 135, 159, 161, 163, 165}) {
        ColorPalette::colorMap[it] = new ColorPalette{it};
        ColorPalette::colorMap[it]->initialize();
    }
    ColorPalette::colorMap[37] = ColorPalette::colorMap[19];
    ColorPalette::colorMap[38] = ColorPalette::colorMap[19];
}
