// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SEVENDAYCOLLECTION_H
#define SEVENDAYCOLLECTION_H

#include <string>
#include <vector>
#include "ui/CardSevenDay.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "util/SevenDay.h"

using std::string;
using std::vector;

class SevenDayCollection {
public:
    SevenDayCollection(Window *, VBox *, SevenDay *);
    void update();

private:
    SevenDay * sevenDay;
    VBox * box;
    Window * parent;
    vector<CardSevenDay> cards;
};

#endif  // SEVENDAYCOLLECTION_H
