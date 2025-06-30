// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/RouteItem.h"

RouteItem::RouteItem(const string& iconString, const string& toolTip, const function<void()>& fn)
    : iconString{iconString}
    , toolTip{toolTip}
    , fn{fn}
{}
