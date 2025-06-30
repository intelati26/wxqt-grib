// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef LSRBYWFO_H
#define LSRBYWFO_H

#include <string>
#include <vector>
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class LsrByWfo : public Window {
public:
    explicit LsrByWfo(Window *);

private:
    void changeSector();
    void getLsrFromWfo();
    void reload();
    void update();
    VBox box;
    VBox boxText;
    HBox boxH;
    ScrolledWindow sw;
    ComboBox comboboxSector;
    vector<string> lsrList;
    vector<Text> textList;
    string wfo;
};

#endif  // LSRBYWFO_H
