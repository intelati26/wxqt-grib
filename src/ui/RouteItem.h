// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef ROUTEITEM_H
#define ROUTEITEM_H

#include <string>
#include <functional>

using std::function;
using std::string;

class RouteItem {
public:
    RouteItem(const string&, const string&, const function<void()>&);
    string iconString;
    string toolTip;
    function<void()> fn;
};

#endif  // ROUTEITEM_H
