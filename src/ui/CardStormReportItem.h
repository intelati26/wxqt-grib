// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDSTORMREPORTITEM_H
#define CARDSTORMREPORTITEM_H

#include <string>
#include "spc/StormReport.h"
#include "ui/Button.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class CardStormReportItem : public HBox {
public:
    CardStormReportItem(Window *, const StormReport&);

private:
    static void launchMap(const string&, const string&);
    VBox box;
    Text text1;
    Text text2;
    Text text3;
    Button button;
};

#endif  // CARDSTORMREPORTITEM_H
