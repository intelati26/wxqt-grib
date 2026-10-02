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
#include "objects/UrlAnimation.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;

class Opc : public Window {
public:
    explicit Opc(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    void closeEventCustom() override;
    void showLatest(const QByteArray&);
    const string prefToken{"OPC_IMG_FAV_URL"};
    ZoomImage image;                 // zoom / pan; a loop through the run of charts (analysis, 24, 48, 96 hour) below it
    UrlAnimation objectAnimate;
    VBox box;
    HBox boxH;
    ComboBox comboBox;
    BackForward backForward;
};

#endif  // OPC_H
