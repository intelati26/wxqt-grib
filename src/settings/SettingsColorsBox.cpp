// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/SettingsColorsBox.h"

SettingsColorsBox::SettingsColorsBox(Window * parent)
    : Widget{parent}
{
    loadColors();
    size_t i = 0;
    for (const auto& color : colors) {
        colorLabels.push_back(std::make_unique<ColorLabel>(parent, color));
        if (i <= static_cast<size_t>(colors.size() * .33)) {
            vbox1.addLayoutReal(colorLabels.back()->getView());
        } else if (i <= static_cast<size_t>(colors.size() * .66)) {
            vbox2.addLayoutReal(colorLabels.back()->getView());
        } else {
            vbox3.addLayoutReal(colorLabels.back()->getView());
        }
        i += 1;
    }
    vbox1.addStretch();
    vbox2.addStretch();
    vbox3.addStretch();
    box.addLayout(vbox1);
    box.addLayout(vbox2);
    box.addLayout(vbox3);
    setLayout(box.getView());
}

void SettingsColorsBox::loadColors() {
    colors.emplace_back("Cities", "RADAR_COLOR_CITY", 255, 255, 255);
    colors.emplace_back("Counties", "RADAR_COLOR_COUNTY", 75, 75, 75);
    colors.emplace_back("County Labels", "RADAR_COLOR_COUNTY_LABELS", 234, 214, 123);
    colors.emplace_back("Dust Storm Warning", "RADAR_COLOR_DSW", 255, 228, 196);
    colors.emplace_back("Flash Flood Warning", "RADAR_COLOR_FFW", 0, 255, 0);
    colors.emplace_back("Hail Indicators", "RADAR_COLOR_HI", 0, 255, 0);
    colors.emplace_back("Highways", "RADAR_COLOR_HW", 135, 135, 135);
    colors.emplace_back("Lakes and Rivers", "RADAR_COLOR_LAKES", 0, 0, 255);
    colors.emplace_back("Location Dot", "RADAR_COLOR_LOCDOT", 255, 255, 255);
    colors.emplace_back("MCD", "RADAR_COLOR_MCD", 153, 51, 255);
    colors.emplace_back("MPD", "RADAR_COLOR_MPD", 0, 255, 0);
    colors.emplace_back("Nexrad Radar Background Color", "NEXRAD_RADAR_BACKGROUND_COLOR", 0, 0, 0);
    colors.emplace_back("NWS Forecast Icon Bottom Color", "NWS_ICON_BOTTOM_COLOR", 255, 255, 255);
    colors.emplace_back("NWS Forecast Icon Text Color", "NWS_ICON_TEXT_COLOR", 38, 97, 139);
    colors.emplace_back("Nexrad Radar Background Color", "NEXRAD_RADAR_BACKGROUND_COLOR", 0, 0, 0);
    colors.emplace_back("Observations", "RADAR_COLOR_OBS", 255, 255, 255);
    colors.emplace_back("Secondary Roads", "RADAR_COLOR_HW_EXT", 91, 91, 91);
    colors.emplace_back("Snow Squall Warning", "RADAR_COLOR_SQW", 199, 21, 133);
    colors.emplace_back("Special Marine Warning", "RADAR_COLOR_SMW", 255, 165, 0);
    colors.emplace_back("Special Weather Statement", "RADAR_COLOR_SPS", 255, 226, 181);
    colors.emplace_back("Spotters", "RADAR_COLOR_SPOTTER", 255, 0, 245);
    colors.emplace_back("State Lines", "RADAR_COLOR_STATE", 142, 142, 142);
    colors.emplace_back("Storm Tracks", "RADAR_COLOR_STI", 255, 255, 255);
    colors.emplace_back("Thunderstorm Warning", "RADAR_COLOR_TSTORM", 255, 255, 0);
    colors.emplace_back("Thunderstorm Watch", "RADAR_COLOR_TSTORM_WATCH", 255, 187, 0);
    colors.emplace_back("Tornado Warning", "RADAR_COLOR_TOR", 243, 85, 243);
    colors.emplace_back("Tornado Watch", "RADAR_COLOR_TOR_WATCH", 255, 0, 0);
    colors.emplace_back("Wind Barbs", "RADAR_COLOR_OBS_WINDBARBS", 255, 255, 255);
}
