// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCFIRESUMMARY_H
#define SPCFIRESUMMARY_H

#include <memory>
#include <string>
#include <vector>
#include <QLabel>
#include "ui/Image.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcFireSummary : public Window {
public:
    explicit SpcFireSummary(Window *);

private:
    void resizeEventCustom() override;
    VBox box;
    vector<string> urls;   // day 1, day 2, then each of days 3 to 8 on its own, four across like the convective summary
    vector<Image> images;
    vector<QLabel *> headers;   // above each picture: the day and (days 1 and 2) its risk on the risk's colour
    Text notes;   // under the pictures: who issued each outlook, when, and the risk
    vector<Shortcut> shortcuts;
};

#endif  // SPCFIRESUMMARY_H
