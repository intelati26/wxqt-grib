// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CMENUITEM_H
#define CMENUITEM_H

#include <functional>
#include <string>

using std::function;
using std::string;

class CMenuItem {
public:
    CMenuItem(const string&, const function<void()>&);
    string label;
    function<void()> fn;
};

#endif  // CMENUITEM_H
