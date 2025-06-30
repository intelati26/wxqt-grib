// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCTSTORMOUTLOOKS_H
#define SPCTSTORMOUTLOOKS_H

#include <vector>
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/Window.h"

using std::vector;

class SpcTstormOutlooks : public Window {
public:
    explicit SpcTstormOutlooks(Window *);

private:
    void resizeEventCustom() override;
    HBox box;
    vector<Image> images;
};

#endif  // SPCTSTORMOUTLOOKS_H
