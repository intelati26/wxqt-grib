// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ColoredBox.h"

ColoredBox::ColoredBox(Window * parent, const WXColor& wxcolor)
    : colorPatchCurrent{new ClickableLabel{parent}}
{
    colorPatchCurrent->setStyleSheet(wxcolor.styleSheetCurrent());
    colorPatchCurrent->setFixedSize(sizeColorBlock, sizeColorBlock);
}

void ColoredBox::regenerate(const WXColor& wxcolor) {
    colorPatchCurrent->setStyleSheet(wxcolor.styleSheetCurrent());
}

void ColoredBox::connect(const function<void()>& fn) {
    colorPatchCurrent->connect([fn] { fn(); });
}

ClickableLabel * ColoredBox::getView() {
    return colorPatchCurrent;
}
