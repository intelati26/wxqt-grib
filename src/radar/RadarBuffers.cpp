// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RadarBuffers.h"
#include "objects/Color.h"
#include "radarcolorpalette/ColorPalette.h"
#include "settings/RadarPreferences.h"

RadarBuffers::RadarBuffers() {
    const auto size = 150'000;
    rectPoints.reserve(size);
    color.reserve(size);
    colorPens.reserve(size);
    colorBrushes.reserve(size);
}

void RadarBuffers::initialize() {
    rectPoints.clear();
    color.clear();
    colorPens.clear();
    colorBrushes.clear();
}

void RadarBuffers::setBackgroundColor() {
    ColorPalette::colorMap[productCode]->redValues.putByIndex(0, Color::red(Color::qcolorToInt(RadarPreferences::nexradRadarBackgroundColor)));
    ColorPalette::colorMap[productCode]->greenValues.putByIndex(0, Color::green(Color::qcolorToInt(RadarPreferences::nexradRadarBackgroundColor)));
    ColorPalette::colorMap[productCode]->blueValues.putByIndex(0, Color::blue(Color::qcolorToInt(RadarPreferences::nexradRadarBackgroundColor)));
}

void RadarBuffers::putColorsByIndex(int level) {
    color.emplace_back(
        ColorPalette::colorMap[productCode]->redValues.getByIndex(level),
        ColorPalette::colorMap[productCode]->greenValues.getByIndex(level),
        ColorPalette::colorMap[productCode]->blueValues.getByIndex(level));
    colorPens.emplace_back(color.back(), 0.0, Qt::SolidLine);
    colorBrushes.emplace_back(color.back(), Qt::SolidPattern);
}
