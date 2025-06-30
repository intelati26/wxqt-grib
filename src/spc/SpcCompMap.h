// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCCOMPMAP_H
#define SPCCOMPMAP_H

#include <string>
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class SpcCompMap : public Window {
public:
    explicit SpcCompMap(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeProduct();
    const string prefToken{"SPCCOMPMAP_LAYERSTRIOS"};
    VBox box;
    HBox boxH;
    Photo photo;
    ComboBox comboBox;
    BackForward backForward;
    string product;
    int index;
};

#endif  // SPCCOMPMAP_H
