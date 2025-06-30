// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WFOSITES_H
#define WFOSITES_H

#include <string>
#include <unordered_map>
#include "objects/Sites.h"

using std::string;
using std::unordered_map;

class WfoSites {
public:
    static void initialize();
    static Sites * sites;

private:
    static const unordered_map<string, string> name;
    static const unordered_map<string, string> lat;
    static const unordered_map<string, string> lon;
};

#endif  // WFOSITES_H
