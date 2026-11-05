// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSWOSUMMARY_H
#define SPCSWOSUMMARY_H

#include <string>
#include <vector>
#include "spc/SpcOutlookNav.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcSwoSummary : public Window {
public:
    explicit SpcSwoSummary(Window *);

private:
    void showTable();
    vector<string> cells = vector<string>(8);   // the highest risk of each day (HTML of its table cell), "" until it is known
    SpcOutlookNav nav;
    Text outlookTable;   // each day's highest risk, coloured, above the pictures
    VBox box;
    ScrolledWindow sw;
    vector<string> urls;
    vector<Image> images;
    const vector<int> day1to3List{1, 2, 3};
    const vector<int> day4To8List{4, 5, 6, 7, 8};
    vector<Shortcut> shortcuts;
};

#endif  // SPCSWOSUMMARY_H
