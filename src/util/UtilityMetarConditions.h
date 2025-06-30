// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMETARCONDITIONS_H
#define UTILITYMETARCONDITIONS_H

#include <string>
#include <unordered_map>

using std::string;
using std::unordered_map;

class UtilityMetarConditions {
public:
    static const unordered_map<string, string> iconFromCondition;
};

#endif  // UTILITYMETARCONDITIONS_H
