// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardBlackHeaderText.h"

CardBlackHeaderText::CardBlackHeaderText(Window * parent, const string& header)
    : text{parent, header}
{
    text.setBlueOnWhite();
    addWidget(text, 1);
}
