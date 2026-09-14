// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/SettingsMain.h"
#include "common/GlobalVariables.h"
#include "util/Utility.h"

using std::make_unique;

SettingsMain::SettingsMain(Window * parent, const function<void()>& reloadFn, bool showLocationItems, bool showRadarFirst, Toolbar * toolbar)
    : Window{parent}
    , reloadFn{reloadFn}
    , sw{this, box}
    , tabWidget{this}
    , settingsBox{make_unique<SettingsBox>(parent)}
    , settingsRadarBox{make_unique<SettingsRadarBox>(parent)}
    , settingsColorsBox{make_unique<SettingsColorsBox>(parent)}
    , settingsAboutBox{make_unique<TextViewerStaticBox>(parent, GlobalVariables::aboutString)}
    , settingsString{getSettings()}
    , shortcutGeneral{QKeySequence{"G"}, this}
    , shortcutRadar{QKeySequence{"R"}, this}
    , shortcutColors{QKeySequence{"C"}, this}
    , shortcutLocations{QKeySequence{"L"}, this}
    , shortcutAddLocation{QKeySequence{"N"}, this}
{
    setTitle("Settings");
    if (showLocationItems) {
        settingsLocationsBox = make_unique<SettingsLocationsBox>(parent);
        locationEditBox = make_unique<LocationEditBox>(parent);
    }
    if (toolbar) {
        settingsToolbarOrderBox = make_unique<SettingsToolbarOrderBox>(parent, toolbar);
    }
    tabWidget.addTab(settingsBox.get(), "General");
    tabWidget.addTab(settingsRadarBox.get(), "Radar");
    tabWidget.addTab(settingsColorsBox.get(), "Colors");
    if (showLocationItems) {
        tabWidget.addTab(settingsLocationsBox.get(), "Locations");
        tabWidget.addTab(locationEditBox.get(), "Add Location");
    }
    if (toolbar) {
        tabWidget.addTab(settingsToolbarOrderBox.get(), "Toolbar Order");
    }
    tabWidget.addTab(settingsAboutBox.get(), "About");
    if (showRadarFirst) {
        tabWidget.setIndex(1);
    }
    if (showLocationItems) {
        tabWidget.connect([this] (int i) {
            if (i == 3) {
                settingsLocationsBox->refresh();
            }
        });
    } else {
        tabWidget.setIndex(1);
    }
    box.addWidget(tabWidget);

    shortcutGeneral.connect([this] { tabWidget.setIndex(0); });
    shortcutRadar.connect([this] { tabWidget.setIndex(1); });
    shortcutColors.connect([this] { tabWidget.setIndex(2); });
    shortcutLocations.connect([this] { tabWidget.setIndex(3); });
    shortcutAddLocation.connect([this] { tabWidget.setIndex(4); });
}

string SettingsMain::getSettings() {
    string s;
    for (const auto& key : Utility::prefGetAllKeys()) {
        s += key + ":" + Utility::readPref(key, "");
    }
    return s;
}

void SettingsMain::closeEventCustom() {
    const auto newSettings = getSettings();
    if (newSettings != settingsString) {
        reloadFn();
    }
}
