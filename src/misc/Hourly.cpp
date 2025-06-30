// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Hourly.h"
#include "objects/FutureText.h"
#include "settings/Location.h"

Hourly::Hourly(Window * parent)
    : Window{parent}
    , sw{this, box}
    , text{this}
{
    setTitle("Hourly forecast for " + Location::name());
    text.setFixedWidth();
    box.addWidget(text);
    box.addMargins();
    new FutureText{this, "HOURLY", [this] (const auto& s) { text.setText(s); }};
}
