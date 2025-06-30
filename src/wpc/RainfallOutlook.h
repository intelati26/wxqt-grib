// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RAINFALLOUTLOOK_H
#define RAINFALLOUTLOOK_H

#include "ui/Photo.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/VBox.h"
#include "ui/Window.h"

class RainfallOutlook : public Window {
public:
    RainfallOutlook(Window *, int);

private:
    VBox vbox0;
    VBox vbox1;
    TwoWidgetScroll sw;
    Photo photo;
    Text text;
    Shortcut shortcutImage;
};

#endif  // RAINFALLOUTLOOK_H
