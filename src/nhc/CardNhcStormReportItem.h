// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDNHCSTORMREPORTITEM_H
#define CARDNHCSTORMREPORTITEM_H

#include "nhc/NhcStormDetails.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

class CardNhcStormReportItem : public HBox {
public:
    CardNhcStormReportItem(Window *, const NhcStormDetails&);

private:
    NhcStormDetails stormData;
    VBox textLayout;
    Button button;
    Image image;
    Text text1;
    Text text2;
    Text text3;
    Text text4;
    Text text5;
};

#endif  // CARDNHCSTORMREPORTITEM_H
