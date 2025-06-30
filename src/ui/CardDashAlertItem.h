// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDDASHALERTITEM_H
#define CARDDASHALERTITEM_H

#include "objects/ObjectWarning.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

class CardDashAlertItem : public HBox {
public:
    CardDashAlertItem(Window *, const ObjectWarning&);

private:
    VBox boxButtons;
    VBox boxText;
    Button buttonDetails;
    Button buttonRadar;
    Text text1;
    Text text2;
    Text text3;
    Text text4;
    Text text5;
};

#endif  // CARDDASHALERTITEM_H
