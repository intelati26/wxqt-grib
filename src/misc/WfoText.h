// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WFOTEXT_H
#define WFOTEXT_H

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

class WfoText : public Window {
public:
    WfoText(Window *, const string& site = "");

private:
    void reload();
    void changeProduct();
    void changeSector();
    VBox box;
    HBox boxH;
    HBox boxText;
    ScrolledWindow sw;
    ComboBox comboboxSector;
    ComboBox comboboxProduct;
    string product{"AFD"};
    string sector;
    vector<Text> textList;
    int productCount{3};
};

#endif  // WFOTEXT_H
