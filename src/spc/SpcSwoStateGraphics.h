// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSWOSTATEGRAPHICS_H
#define SPCSWOSTATEGRAPHICS_H

#include <string>
#include <vector>
#include "ui/ComboBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/Shortcut.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcSwoStateGraphics : public Window {

public:
    SpcSwoStateGraphics(Window *, int);

private:
    void reload();
    void changeState();
    void launchImage(int);
    void resizeEventCustom() override;
    // First of the members, so that it is set before any of the others is built: the scrolled window below shows the window as it is built, and the resize that comes with that
    // reaches resizeEventCustom while images (declared later) does not exist yet. False until the constructor has added the pictures.
    bool ready{false};
    VBox box;
    ScrolledWindow sw;
    ComboBox comboBox;
    int day;
    string state;
    vector<Image> images;
    vector<string> urls;
    vector<Shortcut> shortcuts;
    const int numberAcross{3};
};

#endif  // SPCSWOSTATEGRAPHICS_H
