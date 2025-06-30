// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCFIRESUMMARY_H
#define SPCFIRESUMMARY_H

#include <string>
#include <vector>
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/Shortcut.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcFireSummary : public Window {
public:
    explicit SpcFireSummary(Window *);

private:
    HBox box;
    vector<string> urls;
    vector<Image> images;
    vector<Shortcut> shortcuts;
};

#endif  // SPCFIRESUMMARY_H
