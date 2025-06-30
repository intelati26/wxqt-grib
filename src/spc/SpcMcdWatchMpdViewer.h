// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCMCDWATCHMPDVIEWER_H
#define SPCMCDWATCHMPDVIEWER_H

#include <string>
#include "ui/Button.h"
#include "ui/Photo.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/Window.h"
#include "ui/VBox.h"

using std::string;

class SpcMcdWatchMpdViewer : public Window {
public:
    SpcMcdWatchMpdViewer(Window *, const string&);

private:
    void updateText(const string&);
    static string getToken(const string&);
    Window * parent;
    VBox boxText;
    Text text;
    Photo photo;
    TwoWidgetScroll sw;
    Button button;
    string token;
    string radar;
    Shortcut shortcut;
    Shortcut shortcutImage;
};

#endif  // SPCMCDWATCHMPDVIEWER_H
