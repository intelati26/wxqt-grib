// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef VTECUTIL_H
#define VTECUTIL_H

#include <string>

using std::string;

class VtecUtil {
public:
    static bool isVtecCurrent(const string&);   // a warning's VTEC time range has not ended yet
};

#endif  // VTECUTIL_H
