// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpcSoundings.h"
#include "common/GlobalVariables.h"

string UtilitySpcSoundings::getImage(const string& office) {
    return GlobalVariables::nwsSPCwebsitePrefix + "/exper/soundings/LATEST/" + office + ".gif";
}
