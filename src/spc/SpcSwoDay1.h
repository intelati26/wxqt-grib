// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSWODAY1_H
#define SPCSWODAY1_H

#include <string>
#include <vector>
#include "ui/Button.h"
#include "ui/Image.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcSwoDay1 : public Window {
public:
    SpcSwoDay1(Window *, int);

private:
    VBox imageVBox;
    VBox boxText;
    TwoWidgetScroll sw;
    Text text;
    Button button;
    Shortcut launchStateShortcut;
    vector<Shortcut> shortcuts;
    vector<string> urls;
    vector<Image> images;
};

#endif  // SPCSWODAY1_H
