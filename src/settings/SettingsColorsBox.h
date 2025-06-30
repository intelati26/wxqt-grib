// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SETTINGSCOLORSBOX_H
#define SETTINGSCOLORSBOX_H

#include <memory>
#include <vector>
#include "objects/WXColor.h"
#include "ui/HBox.h"
#include "ui/ColorLabel.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::vector;

class SettingsColorsBox : public Widget {
public:
    explicit SettingsColorsBox(Window *);

private:
    void loadColors();
    HBox box;
    VBox vbox1;
    VBox vbox2;
    VBox vbox3;
    vector<WXColor> colors;
    vector<std::unique_ptr<ColorLabel>> colorLabels;
};

#endif  // SETTINGSCOLORSBOX_H
