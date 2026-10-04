// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADUTIL_H
#define NEXRADUTIL_H

#include <string>

using std::string;

class NexradUtil {
public:
    static bool isVtecCurrent(const string&);   // a warning's VTEC time range has not ended yet
};

#endif  // NEXRADUTIL_H
