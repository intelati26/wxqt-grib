// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RAINFALLOUTLOOKSUMMARY_H
#define RAINFALLOUTLOOKSUMMARY_H

#include <string>
#include <vector>
#include "ui/Image.h"
#include <QLabel>
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class RainfallOutlookSummary : public Window {
public:
    explicit RainfallOutlookSummary(Window *);

private:
    void resizeEventCustom() override;
    VBox box;
    vector<Image> images;
    vector<QLabel *> headers;   // above each picture: the day and its highest category on WPC's colour for it
    Text notes;   // under the pictures: when each day's outlook was issued and what period it is for
    vector<string> dayText;   // each day's line of the notes, once its outlook has come
    vector<string> urls;
    const int numberAcross{3};
    vector<Shortcut> shortcuts;
};

#endif  // RAINFALLOUTLOOKSUMMARY_H
