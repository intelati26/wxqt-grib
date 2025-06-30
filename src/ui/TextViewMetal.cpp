// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "TextViewMetal.h"

double TextViewMetal::fontSize{8.0};

TextViewMetal::TextViewMetal(double xPos, double yPos, const string& text)
    : xPos{static_cast<int>(xPos)}
    , yPos{static_cast<int>(yPos)}
    , text{text}
{}
