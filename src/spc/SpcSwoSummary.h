// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSWOSUMMARY_H
#define SPCSWOSUMMARY_H

#include <string>
#include <vector>
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/Shortcut.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcSwoSummary : public Window {
public:
    explicit SpcSwoSummary(Window *);

private:
    VBox box;
    ScrolledWindow sw;
    vector<string> urls;
    vector<Image> images;
    const vector<int> day1to3List{1, 2, 3};
    const vector<int> day4To8List{4, 5, 6, 7, 8};
    vector<Shortcut> shortcuts;
};

#endif  // SPCSWOSUMMARY_H
