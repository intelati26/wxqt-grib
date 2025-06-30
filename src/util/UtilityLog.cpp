// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityLog.h"
#include <iostream>
#include "objects/ObjectDateTime.h"

void UtilityLog::d(const string& s) {
    std::cout << ObjectDateTime::getLocalTimeAsString() << " " << s << std::endl;
}
