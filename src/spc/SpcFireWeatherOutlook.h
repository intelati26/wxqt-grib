// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCFIREWEATHEROUTLOOK_H
#define SPCFIREWEATHEROUTLOOK_H

#include "ui/Photo.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/Window.h"

class SpcFireWeatherOutlook : public Window {
public:
    SpcFireWeatherOutlook(Window *, int);

private:
    Photo photo;
    Text text;
    TwoWidgetScroll sw;
    Shortcut shortcutImage;
};

#endif  // SPCFIREWEATHEROUTLOOK_H
