// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RadarBuffers.h"
#include <algorithm>
#include "objects/Color.h"
#include "radarcolorpalette/ColorPalette.h"
#include "settings/RadarPreferences.h"

RadarBuffers::RadarBuffers() {
    const auto size = 150'000;
    rectPoints.reserve(size);
    colorBrushes.reserve(size);
}

void RadarBuffers::initialize() {
    rectPoints.clear();
    colorBrushes.clear();
    brushMade.fill(false);
}

void RadarBuffers::setBackgroundColor() {
    ColorPalette::colorMap[productCode]->redValues.putByIndex(0, Color::red(Color::qcolorToInt(RadarPreferences::nexradRadarBackgroundColor)));
    ColorPalette::colorMap[productCode]->greenValues.putByIndex(0, Color::green(Color::qcolorToInt(RadarPreferences::nexradRadarBackgroundColor)));
    ColorPalette::colorMap[productCode]->blueValues.putByIndex(0, Color::blue(Color::qcolorToInt(RadarPreferences::nexradRadarBackgroundColor)));
}

void RadarBuffers::putColorsByIndex(int level) {
    const auto index = static_cast<size_t>(std::clamp(level, 0, 255));
    if (!brushMade[index]) {
        brushOfLevel[index] = QBrush{QColor{
            ColorPalette::colorMap[productCode]->redValues.getByIndex(level),
            ColorPalette::colorMap[productCode]->greenValues.getByIndex(level),
            ColorPalette::colorMap[productCode]->blueValues.getByIndex(level)}, Qt::SolidPattern};
        brushMade[index] = true;
    }
    colorBrushes.push_back(brushOfLevel[index]);
}
