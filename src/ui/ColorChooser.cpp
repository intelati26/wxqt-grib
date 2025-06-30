// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <QColorDialog>
#include "ColorChooser.h"
#include "objects/Color.h"
#include "settings/RadarPreferences.h"

ColorChooser::ColorChooser(Window * parent, WXColor * wxcolor, ColoredBox * colorPatchCurrent)
    : parent{parent}
    , wxcolor{wxcolor}
    , colorPatchCurrent{colorPatchCurrent}
{}

void ColorChooser::run() {
    const auto qColorDialog = new QColorDialog{Color::qtBlack(), parent};
    const auto color = qColorDialog->getColor(wxcolor->qcolorDefault);
    if (color.isValid()) {
        wxcolor->red = color.red();
        wxcolor->green = color.green();
        wxcolor->blue = color.blue();
        wxcolor->setValue(Color::qcolorToInt(color));
        colorPatchCurrent->regenerate(*wxcolor);
        RadarPreferences::initialize();
    }
}
