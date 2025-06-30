// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TEXTVIEWER_H
#define TEXTVIEWER_H

#include <string>
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class TextViewer : public Window {
public:
    TextViewer(Window *, const string&, bool, const string& = "", int = 0, int = 0);

private:
    VBox box;
    ScrolledWindow sw;
    Text text;
};

#endif  // TEXTVIEWER_H
