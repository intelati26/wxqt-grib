// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDCURRENTCONDITIONS_H
#define CARDCURRENTCONDITIONS_H

#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "util/CurrentConditions.h"

class CardCurrentConditions : public HBox {
public:
    CardCurrentConditions(Window *, const CurrentConditions&);
    void update(const CurrentConditions&);

private:
    VBox boxText;
    VBox boxImage;
    Text text1;
    Text text2;
    Text text3;
    Text text4;
    Text text5;
    Photo photo;
};

#endif  // CARDCURRENTCONDITIONS_H
