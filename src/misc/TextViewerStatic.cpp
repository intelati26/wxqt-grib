// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "TextViewerStatic.h"

TextViewerStatic::TextViewerStatic(Window * parent, const string& html, const string& title, int width, int height)
    : Window{parent}
    , sw{this, box}
    , text{this, html}
{
    setTitle(title);
    if (width != 0 && height != 0) {
        setSize(width, height);
    }
    box.addMargins();
    box.addWidget(text);
}
