// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COLORCHOOSER_H
#define COLORCHOOSER_H

#include "objects/WXColor.h"
#include "ui/ColoredBox.h"
#include "ui/Window.h"

class ColorChooser {
public:
    ColorChooser(Window *, WXColor *, ColoredBox *);
    void run();

private:
    Window * parent;
    WXColor * wxcolor;
    ColoredBox * colorPatchCurrent;
};

#endif  // COLORCHOOSER_H
