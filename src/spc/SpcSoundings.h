// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSOUNDINGS_H
#define SPCSOUNDINGS_H

#include <string>
#include "ui/BackForward.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class SpcSoundings : public Window {
public:
    SpcSoundings(Window *, const string& = "");

private:
    void moveBack();
    void moveForward();
    void reload();
    void changeProduct();
    void launchText();
    string office;
    int index;
    Photo photo;
    BackForward backForward;
    Button buttonForText;
    VBox box;
    HBox boxH;
    ComboBox comboboxProduct;
};

#endif  // SPCSOUNDINGS_H
