// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBSERVATIONS_H
#define OBSERVATIONS_H

#include <string>
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class Observations : public Window {
public:
    explicit Observations(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    const string prefToken{"SFC_OBS_IMG_IDX"};
    Photo photo;
    HBox boxH;
    VBox box;
    ComboBox comboBox;
    BackForward backForward;
};

#endif  // OBSERVATIONS_H
