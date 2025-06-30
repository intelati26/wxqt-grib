// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "AlertsDetail.h"
#include "objects/FutureVoid.h"

AlertsDetail::AlertsDetail(Window * parent, const string& url)
    : Window{parent}
    , sw{this, box}
    , text{this}
    , url{url}
{
    box.addMargins();
    box.addWidget(text);
    new FutureVoid{this, [this, url] { capAlert = CapAlert{url}; }, [this] { update(); }};
}

void AlertsDetail::update() {
    setTitle(capAlert.title);
    text.setText(capAlert.text);
}
