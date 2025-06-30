// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDALERTDETAILS_H
#define CARDALERTDETAILS_H

#include "misc/CapAlertXml.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

class CardAlertDetail : public HBox {
public:
    CardAlertDetail(Window *, const CapAlertXml&);

private:
    VBox layoutVertical;
    VBox boxText;
    Button buttonDetails;
    Button buttonRadar;
    Text text1;
    Text text2;
    Text text3;
    Text text4;
};

#endif  // CARDALERTDETAILS_H
