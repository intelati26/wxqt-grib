// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/SettingsBox.h"
#include "common/GlobalVariables.h"
#include "gfs/GfsRender.h"
#include "misc/TextViewerStatic.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityTheme.h"

SettingsBox::SettingsBox(Window * parent)
    : Widget{parent}
    , button{parent, None, "Keyboard Shortcuts"}
    , buttonClearCache{parent, None, "Clear the model data cache"}
    , cacheUsage{parent, ""}
    , homeScreenLabel{parent, "Homescreen widgets: choose and order them under Home Screen Order (drag, tick)."}
    , generalLabel{parent, "General preferences:"}
    , themeLabel{parent, "Theme (light / dark)"}
    , themeComboBox{parent, UtilityTheme::labels}
    , contactEmailLabel{parent, "Contact email for weather API requests (optional)"}
    , contactEmailEntry{parent}
{
    boxMain.setSpacing(10);
    boxMain.addLayout(boxCenter);
    boxMain.addLayout(boxRight);
    setLayout(boxMain.getView());

    // configs.push_back(std::make_unique<Switch>(parent, "Use new NWS API", "USE_NWS_API_SEVEN_DAY", true));
    // configs.push_back(std::make_unique<Switch>(parent, "Use new NWS API - Hourly", "USE_NWS_API_HOURLY", true));

    configs.push_back(std::make_unique<Switch>(parent, "Show mini SevereDashboard on main screen", "MAINSCREEN_SEVERE_DASH", false));
    configs.push_back(std::make_unique<Switch>(parent, "Show the hourly graph on the home screen", "HOURLY_GRAPH", true));
    configs.push_back(std::make_unique<Switch>(parent, "Hourly graph above the seven day forecast (off: below it)", "HOURLY_GRAPH_ABOVE", true));
    configs.push_back(std::make_unique<Switch>(parent, "Captions under the home screen pictures", "HOME_CAPTIONS", true));
    configs.push_back(std::make_unique<Switch>(parent, "Toggle scroll wheel motion", "NEXRAD_SCROLLWHEEL", false));
    configs.push_back(std::make_unique<Switch>(parent, "Remember last GOES image", "REMEMBER_GOES", false));
    configs.push_back(std::make_unique<Switch>(parent, "Remember last Radar Mosaic image", "REMEMBER_MOSAIC", false));
    configs.push_back(std::make_unique<Switch>(parent, "Tiled Windows", "TILED_WINDOWS", false));

    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Main screen image size", "MAIN_SCREEN_IMAGE_SIZE", 400, 200, 800, 25));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Main Screen update refresh interval (in minutes)", "MAIN_SCREEN_DATA_REFRESH_INTERVAL", 10, 1, 60, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Toolbar icon size", "TOOLBAR_ICON_SIZE", 36, 10, 72, 4));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Forecast icon size", "NWS_ICON_SIZE_PREF", 62, 10, 120, 4));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Font size", "GENERAL_FONT_SIZE", 13, 6, 30, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Model data kept on disk for this many hours (applied when the program starts)", "MODEL_CACHE_HOURS", 48, 1, 720, 6));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Model data cache size limit (MB; the oldest go first)", "MODEL_CACHE_MB", 2048, 200, 20000, 200));

    button.connect([parent] { new TextViewerStatic{parent, GlobalVariables::mainScreenShortcuts}; });

    homeScreenLabel.setBlue();
    homeScreenLabel.setWordWrap(false);
    boxCenter.addWidget(homeScreenLabel);   // a pointer to where the home screen pictures are chosen

    generalLabel.setBlue();
    boxCenter.addWidget(generalLabel);

    themeLabel.setWordWrap(false);
    themeComboBox.setIndex(UtilityTheme::prefIndex());
    themeComboBox.connect([this] { changeTheme(); });
    themeRow.addWidget(themeComboBox);
    themeRow.addWidget(themeLabel);
    boxCenter.addLayout(themeRow);

    contactEmailLabel.setWordWrap(true);
    contactEmailEntry.setText(Utility::readPref("CONTACT_EMAIL", ""));
    contactEmailEntry.connect([this] { changeContactEmail(); });
    boxCenter.addWidget(contactEmailLabel);
    boxCenter.addWidget(contactEmailEntry);

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
    cacheUsage.setWordWrap(true);
    boxRight.addWidget(cacheUsage);
    boxRight.addWidget(buttonClearCache);
    buttonClearCache.connect([this] {
        GfsRender::clearCache();
        showCacheUsage();
    });
    showCacheUsage();
    boxLeft.addStretch();
    boxCenter.addStretch();
    boxRight.addStretch();
}

void SettingsBox::showCacheUsage() {
    const auto megabytes = static_cast<double>(GfsRender::cacheUsage()) / (1024.0 * 1024.0);
    cacheUsage.setText(string{"The model data cache (the fields of the GFS, RRFS, NBM and the other models drawn from GRIB) holds "} + std::to_string(static_cast<long long>(megabytes + 0.5)) + " MB.");
}

void SettingsBox::changeTheme() {
    auto index = themeComboBox.getIndex();
    if (index < 0 || index >= static_cast<int>(UtilityTheme::values.size())) {
        index = 0;
    }
    const auto value = UtilityTheme::values[index];
    Utility::writePref(UtilityTheme::pref, value);
    UtilityTheme::applyTheme(value);
}

void SettingsBox::changeContactEmail() {
    Utility::writePref("CONTACT_EMAIL", contactEmailEntry.getText());
}
