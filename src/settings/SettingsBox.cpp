// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/SettingsBox.h"
#include "common/GlobalVariables.h"
#include "misc/TextViewerStatic.h"
#include "settings/UIPreferences.h"
#include "util/UtilityList.h"

SettingsBox::SettingsBox(Window * parent)
    : Widget{parent}
    , button{parent, None, "Keyboard Shortcuts"}
    , homeScreenLabel{parent, "Homescreen widgets:"}
    , generalLabel{parent, "General preferences:"}
{
    boxMain.setSpacing(10);
    boxMain.addLayout(boxLeft);
    boxMain.addLayout(boxCenter);
    boxMain.addLayout(boxRight);
    setLayout(boxMain.getView());

    configs.push_back(std::make_unique<Switch>(parent, "Show Nexrad on main screen", "NEXRAD_ON_MAIN_SCREEN", false));
    // configs.push_back(std::make_unique<Switch>(parent, "Use new NWS API", "USE_NWS_API_SEVEN_DAY", true));
    // configs.push_back(std::make_unique<Switch>(parent, "Use new NWS API - Hourly", "USE_NWS_API_HOURLY", true));

    configs.push_back(std::make_unique<Switch>(parent, "Show mini SevereDashboard on main screen", "MAINSCREEN_SEVERE_DASH", false));
    configs.push_back(std::make_unique<Switch>(parent, "Toggle scroll wheel motion", "NEXRAD_SCROLLWHEEL", false));
    configs.push_back(std::make_unique<Switch>(parent, "Remember last GOES image", "REMEMBER_GOES", false));
    configs.push_back(std::make_unique<Switch>(parent, "Remember last Radar Mosaic image", "REMEMBER_MOSAIC", false));
    configs.push_back(std::make_unique<Switch>(parent, "Tiled Windows", "TILED_WINDOWS", false));

    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Main screen image size", "MAIN_SCREEN_IMAGE_SIZE", 400, 200, 800, 25));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Main Screen update refresh interval (in minutes)", "MAIN_SCREEN_DATA_REFRESH_INTERVAL", 10, 1, 60, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Toolbar icon size", "TOOLBAR_ICON_SIZE", 36, 10, 72, 4));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Forecast icon size", "NWS_ICON_SIZE_PREF", 62, 10, 120, 4));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Font size", "GENERAL_FONT_SIZE", 13, 6, 30, 1));

    button.connect([parent] { new TextViewerStatic{parent, GlobalVariables::mainScreenShortcuts}; });

    homeScreenLabel.setBlue();
    homeScreenLabel.setWordWrap(false);
    boxLeft.addWidget(homeScreenLabel);

    for (const auto& item : UIPreferences::homeScreenItemsImage) {
        auto sw = Switch::fromPrefBool(parent, item);
        boxLeft.addWidget(*sw);
    }

    for (const auto& item : UIPreferences::homeScreenItemsText) {
        auto sw = Switch::fromPrefBool(parent, item);
        boxLeft.addWidget(*sw);
    }
    generalLabel.setBlue();
    boxCenter.addWidget(generalLabel);

    for (auto i : range(configsLeft.size())) {
        boxLeft.addWidget(*configsLeft[i]);
    }
    for (auto i : range(configs.size())) {
        boxCenter.addWidget(*configs[i]);
    }

    boxRight.addWidget(button);
    for (auto i : range(numberPickers.size())) {
        boxRight.addLayout(*numberPickers[i]);
    }
    boxLeft.addStretch();
    boxCenter.addStretch();
    boxRight.addStretch();
}
