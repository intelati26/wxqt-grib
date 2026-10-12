// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCFIREWEATHEROUTLOOK_H
#define SPCFIREWEATHEROUTLOOK_H

#include <string>
#include <vector>
#include "ui/ComboBox.h"
#include "ui/Photo.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// One screen for all of the SPC's fire weather outlooks, as its fire weather page has them: the day 1 and 2 outlooks, the day 3-8 outlook and each of days 3 to 8 on its own, the
// categorical graphic and (days 3-8) the probability graphic, with who issued it and when, what the lines mean, and the text product.
class SpcFireWeatherOutlook : public Window {
public:
    SpcFireWeatherOutlook(Window *, int);   // the day to start on: 0 day 1, 1 day 2, 2 days 3-8

private:
    void showDay(int index);
    VBox leftBox;
    ComboBox comboDay;
    ComboBox comboGraphic;
    Text header;
    Photo photo;
    Text text;
    TwoWidgetScroll sw;
    Shortcut shortcutImage;
    std::string issuance[3];   // "Forecaster: WILLIAMS   Issued: 111618Z   Valid: 111700Z - 121200Z   Risk: Elevated", for day 1, day 2, days 3-8
    std::string productText;
    int generation{0};
};

#endif  // SPCFIREWEATHEROUTLOOK_H
