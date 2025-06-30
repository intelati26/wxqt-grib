// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDLOCATIONITEM_H
#define CARDLOCATIONITEM_H

#include "objects/LatLon.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

class CardLocationItem : public VBox {
public:
    CardLocationItem(Window *, int);

private:
    VBox box;
    LatLon latLon;
    Text text1;
    Text text2;
    Text text3;
};

#endif  // CARDLOCATIONITEM_H
