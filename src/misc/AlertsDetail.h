// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef ALERTSDETAIL_H
#define ALERTSDETAIL_H

#include <string>
#include "misc/CapAlert.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class AlertsDetail : public Window {
public:
    AlertsDetail(Window *, const string&);

private:
    void update();
    VBox box;
    ScrolledWindow sw;
    Text text;
    CapAlert capAlert;
    string url;
};

#endif  // ALERTSDETAIL_H
