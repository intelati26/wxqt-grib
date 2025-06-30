// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ColorLabel.h"
#include "ui/ColorChooser.h"

ColorLabel::ColorLabel(Window * parent, const WXColor& wxcolor)
    : parent{parent}
    , wxcolor{wxcolor}
    , colorPatchCurrent{parent, wxcolor}
    , text{parent, wxcolor.uiLabel}
{
    colorPatchCurrent.getView()->connect([this] { launchColorPicker(); });
    addWidget(colorPatchCurrent);
    addWidget(text);
}

void ColorLabel::launchColorPicker() {
    auto chooser{ColorChooser{parent, &wxcolor, &colorPatchCurrent}};
    chooser.run();
}
