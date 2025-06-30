// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADLONGPRESSMENU_H
#define NEXRADLONGPRESSMENU_H

#include <functional>
#include <string>
#include <QPoint>
#include "objects/LatLon.h"
#include "radar/NexradState.h"
#include "ui/Window.h"

using std::function;
using std::string;

class NexradLongPressMenu {
public:
    static void setupContextMenu(
        Window *,
        const QPoint&,
        const NexradState&,
        const LatLon&,
        const function<void(int, const string&)>&,
        const function<void(int, const string&)>&
    );
};

#endif  // NEXRADLONGPRESSMENU_H
