// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "misc/TextViewerStaticBox.h"

TextViewerStaticBox::TextViewerStaticBox(Window * parent, const string& html)
    : Widget{parent}
    , text{parent, html}
{
    setLayout(box.getView());
    box.addMargins();
    box.addWidget(text);
}
