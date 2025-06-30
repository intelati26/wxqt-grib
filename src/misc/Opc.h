// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OPC_H
#define OPC_H

#include <string>
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class Opc : public Window {
public:
    explicit Opc(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    const string prefToken{"OPC_IMG_FAV_URL"};
    Photo photo;
    VBox box;
    HBox boxH;
    ComboBox comboBox;
    BackForward backForward;
};

#endif  // OPC_H
