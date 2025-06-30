// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COLOREDBOX_H
#define COLOREDBOX_H

#include <functional>
#include <QLabel>
#include "objects/WXColor.h"
#include "ui/ClickableLabel.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;

class ColoredBox : public Widget2 {
public:
    ColoredBox(Window *, const WXColor&);
    void regenerate(const WXColor&);
    void connect(const function<void()>&);
    ClickableLabel * getView();

private:
    ClickableLabel * colorPatchCurrent;
    int sizeColorBlock{30};
};

#endif  // COLOREDBOX_H
