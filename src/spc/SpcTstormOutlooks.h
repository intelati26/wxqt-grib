// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCTSTORMOUTLOOKS_H
#define SPCTSTORMOUTLOOKS_H

#include <vector>
#include <QLabel>
#include "ui/VBox.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Image.h"
#include "ui/Window.h"

using std::vector;

class SpcTstormOutlooks : public Window {
public:
    explicit SpcTstormOutlooks(Window *);

private:
    void resizeEventCustom() override;
    VBox box;
    vector<Image> images;
    vector<QLabel *> headers;   // above each picture: its period and the highest probability in it, on the colour the outlook gives that probability
};

#endif  // SPCTSTORMOUTLOOKS_H
