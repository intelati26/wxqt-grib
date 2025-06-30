// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CMenuItem.h"

CMenuItem::CMenuItem(const string& label, const function<void()>& fn)
    : label{label}
    , fn{fn}
{}
