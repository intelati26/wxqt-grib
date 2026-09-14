// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SETTINGSBOX_H
#define SETTINGSBOX_H

#include <memory>
#include <vector>
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/Entry.h"
#include "ui/HBox.h"
#include "ui/Switch.h"
#include "ui/NumberPicker.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::vector;

class SettingsBox : public Widget {
public:
    explicit SettingsBox(Window *);

private:
    void changeTheme();
    void changeContactEmail();
    vector<std::unique_ptr<Switch>> configsLeft;
    vector<std::unique_ptr<Switch>> configs;
    Button button;
    VBox boxLeft;
    VBox boxCenter;
    VBox boxRight;
    HBox boxMain;
    HBox themeRow;
    Text homeScreenLabel;
    Text generalLabel;
    Text themeLabel;
    ComboBox themeComboBox;
    vector<std::unique_ptr<NumberPicker>> numberPickers;
    // Optional - sent as part of the User-Agent header on requests to NWS/
    // NOAA data sources per their API usage guidelines (a contactable
    // User-Agent, not a mandatory field). Empty by default - never baked
    // into source. See GlobalVariables::appOrgName's comment for why this
    // is a separate preference rather than reusing the app's QSettings key.
    Text contactEmailLabel;
    Entry contactEmailEntry;
};

#endif  // SETTINGSBOX_H
