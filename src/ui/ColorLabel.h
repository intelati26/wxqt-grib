// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COLORLABEL_H
#define COLORLABEL_H

#include "objects/WXColor.h"
#include "ui/ColoredBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/Window.h"

class ColorLabel : public HBox {
public:
    ColorLabel(Window *, const WXColor&);

private:
    void launchColorPicker();
    Window * parent;
    WXColor wxcolor;
    ColoredBox colorPatchCurrent;
    Text text;
};

#endif  // COLORLABEL_H
