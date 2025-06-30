// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBSERVATIONSITES_H
#define OBSERVATIONSITES_H

#include <string>
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/WebViewer.h"
#include "ui/Window.h"

using std::string;

class ObservationSites : public Window {
public:
    explicit ObservationSites(Window *);

private:
    static void launchAlert(const string&);
    VBox box;
    HBox boxH;
    Button button1;
    Button button2;
    string layers;
    string url1;
    string url2;
    WebViewer webViewer1;
    WebViewer webViewer2;
};

#endif  // OBSERVATIONSITES_H
