// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef GLOBALDICTIONARIES_H
#define GLOBALDICTIONARIES_H

#include <string>
#include <unordered_map>
#include <cstdint>

using std::string;
using std::unordered_map;

class GlobalDictionaries {
public:
    static const unordered_map<string, string> nexradProductString;
    static const unordered_map<string, int16_t> radarProductStringToShortInt;
};

#endif  // GLOBALDICTIONARIES_H
