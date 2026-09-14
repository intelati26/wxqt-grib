// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SETTINGSMAIN_H
#define SETTINGSMAIN_H

#include <functional>
#include <memory>
#include <string>
#include "misc/TextViewerStaticBox.h"
#include "settings/LocationEditBox.h"
#include "settings/SettingsBox.h"
#include "settings/SettingsColorsBox.h"
#include "settings/SettingsLocationsBox.h"
#include "settings/SettingsRadarBox.h"
#include "settings/SettingsToolbarOrderBox.h"
#include "ui/ComboBox.h"
#include "ui/ScrolledWindow.h"
#include "ui/Shortcut.h"
#include "ui/TabWidget.h"
#include "ui/Toolbar.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::function;
using std::string;
using std::unique_ptr;

class SettingsMain : public Window {
public:
    SettingsMain(Window *, const function<void()>&, bool, bool, Toolbar * = nullptr);

private:
    static string getSettings();
    void closeEventCustom() override;
    function<void()> reloadFn;
    VBox box;
    ScrolledWindow sw;
    TabWidget tabWidget;
    unique_ptr<SettingsBox> settingsBox;
    unique_ptr<SettingsRadarBox> settingsRadarBox;
    unique_ptr<SettingsColorsBox> settingsColorsBox;
    unique_ptr<SettingsLocationsBox> settingsLocationsBox;
    unique_ptr<LocationEditBox> locationEditBox;
    unique_ptr<SettingsToolbarOrderBox> settingsToolbarOrderBox;
    unique_ptr<TextViewerStaticBox> settingsAboutBox;
    string settingsString;
    Shortcut shortcutGeneral;
    Shortcut shortcutRadar;
    Shortcut shortcutColors;
    Shortcut shortcutLocations;
    Shortcut shortcutAddLocation;
};

#endif  // SETTINGSMAIN_H
