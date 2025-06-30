// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SETTINGSRADARBOX_H
#define SETTINGSRADARBOX_H

#include <memory>
#include <string>
#include <vector>
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/NumberPicker.h"
#include "ui/Switch.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SettingsRadarBox : public Widget {
public:
    explicit SettingsRadarBox(Window *);

private:
    void changeRefPal();
    void changeVelPal();
    void launchShortcuts();
    Window * parent;
    const vector<string> refPalChoices{"CODENH", "DKenh", "NSSL", "NWSD", "GREEN", "AF", "EAK", "NWS"};
    const vector<string> velPalChoices{"CODENH", "EAK", "AF"};
    VBox box;
    VBox vbox0;
    VBox vbox1;
    VBox vbox2;
    HBox hbox;
    HBox hbox0;
    HBox hbox1;
    HBox hbox2;
    HBox hboxBottom;
    Button button;
    Text text1;
    Text text2;
    vector<std::unique_ptr<Switch>> alertConfigs;
    vector<std::unique_ptr<NumberPicker>> numberPickers;
    ComboBox comboBoxRefPal;
    ComboBox comboBoxVelPal;
};

#endif  // SETTINGSRADARBOX_H
