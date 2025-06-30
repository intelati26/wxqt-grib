// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/ObjectAnimateParent.h"
#include "util/Utility.h"
#include "util/UtilityLog.h"

ObjectAnimateParent::ObjectAnimateParent(Window * parent)
    : parent{parent}
    , button{parent, Play, "Animate ctrl-A"}
{
    button.connect([this] { animateClicked(); });
}

void ObjectAnimateParent::setVisible(bool b) {
    button.setVisible(b);
}

void ObjectAnimateParent::setFrameCount(int i) {
    frameCount = i;
}

// void ObjectAnimateParent::setActive() {
//     button.setActive(!button.getActive());
// }

QPushButton * ObjectAnimateParent::getView() {
    return button.getView();
}
