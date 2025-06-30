// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYHOURLY_H
#define UTILITYHOURLY_H

#include <string>
#include <unordered_map>

using std::string;
using std::unordered_map;

class UtilityHourly {
public:
    static string get(int);

private:
    static const unordered_map<string, string> hourlyAbbreviations;
    static string getFooter();
    static string getHourlyString(int);
    static string parse(const string&);
    static string shortenConditions(const string&);
};

#endif  // UTILITYHOURLY_H
