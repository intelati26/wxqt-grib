// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDBLACKHEADERTEXT_H
#define CARDBLACKHEADERTEXT_H

#include <string>
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/Window.h"

using std::string;

class CardBlackHeaderText : public HBox {
public:
    CardBlackHeaderText(Window *, const string&);

private:
    Text text;
};

#endif  // CARDBLACKHEADERTEXT_H
