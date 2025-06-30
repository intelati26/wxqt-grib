// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "util/UtilitySet.h"
#include <algorithm>
#include <iostream>
#include "objects/WString.h"

void UtilitySet::checkEquality(const set<string>& a, const set<string>& b, const string& setName1, const string& setName2) {
    if (a != b) {
        std::cout << "mismatch between " << setName1 << " and " << setName2 << std::endl;
        std::cout << a.size() << std::endl;
        std::cout << b.size() << std::endl;

        std::set<string> difference;
        std::set_difference(
            a.begin(), a.end(), b.begin(), b.end(),
            std::inserter(difference, difference.begin()));
        std::cout << "Set after Difference Name - " << setName2 << ": " << std::endl;
        for (auto const& elem : difference) {
            std::cout << elem << " ";
        }
        std::cout << std::endl;

        std::set<string> difference2;
        std::set_difference(
            b.begin(), b.end(), a.begin(), a.end(),
            std::inserter(difference, difference.begin()));
        std::cout << "Set after Difference Lat - " << setName1 << ": " << std::endl;
        for (auto const& elem : difference) {
            std::cout << elem << " ";
        }
        std::cout << std::endl;
    }
}
