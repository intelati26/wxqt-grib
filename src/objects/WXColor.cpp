// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/WXColor.h"
#include "objects/Color.h"
#include "settings/RadarPreferences.h"
#include "util/To.h"
#include "util/Utility.h"

WXColor::WXColor(const string& uiLabel, const string& prefVar, int defaultRed, int defaultGreen, int defaultBlue)
    : uiLabel{uiLabel}
    , prefVar{prefVar}
    , defaultcolorAsInt{Color::rgb(defaultRed, defaultGreen, defaultBlue)}
    , colorAsInt{Utility::readPrefInt(prefVar, Color::rgb(defaultRed, defaultGreen, defaultBlue))}
    , red{Color::red(colorAsInt)}
    , green{Color::green(colorAsInt)}
    , blue{Color::blue(colorAsInt)}
    , qcolor{Color::intToQColor(colorAsInt)}
    , qcolorDefault{Color::intToQColor(defaultcolorAsInt)}
{}

void WXColor::setValue(int newValue) const {
    Utility::writePref(prefVar, To::string(newValue));
    RadarPreferences::initializeColors();
}

QString WXColor::styleSheetCurrent() const {
    return QString::fromStdString("QLabel { background-color : rgb(" + To::string(red) + "," + To::string(green) + "," + To::string(blue) + ");}");
}
