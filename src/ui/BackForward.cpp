// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/BackForward.h"

BackForward::BackForward(Window * parent, const function<void()>& moveBack, const function<void()>& moveForward)
    : buttonBack{parent, Left, ""}
    , buttonForward{parent, Right, ""}
    , shortcutBack{QKeySequence{"J"}, parent}
    , shortcutForward{QKeySequence{"K"}, parent}
{
    shortcutBack.connect([moveBack] { moveBack(); });
    shortcutForward.connect([moveForward] { moveForward(); });

    buttonBack.connect([moveBack] { moveBack(); });
    buttonForward.connect([moveForward] { moveForward(); });

    addWidget(buttonBack);
    addWidget(buttonForward);

    setSpacing(0);
}
