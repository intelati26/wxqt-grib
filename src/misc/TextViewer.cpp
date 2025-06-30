// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "TextViewer.h"
#include "objects/FutureText.h"

TextViewer::TextViewer(Window * parent, const string& url, bool fixedWidth, const string& title, int width, int height)
    : Window{parent}
    , sw{this, box}
    , text{this}
{
    setTitle(title);
    if (width == 0 && height == 0) {
        maximize();
    } else {
        setSize(width, height);
    }
    if (fixedWidth) {
        text.setFixedWidth();
    }
    box.addMargins();
    box.addWidget(text);
    new FutureText{this, url, [this] (const auto& s) { text.setText(s); }};
}
