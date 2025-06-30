// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSET_H
#define UTILITYSET_H

#include <set>
#include <string>

using std::set;
using std::string;

class UtilitySet {
public:
    static void checkEquality(const set<string>&, const set<string>&, const string&, const string&);
};

#endif  // UTILITYSET_H
