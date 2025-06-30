// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYHOURLYOLDAPI_H
#define UTILITYHOURLYOLDAPI_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityHourlyOldApi {
public:
    static string getHourlyString(int);

private:
    static const vector<string> regexpList;
    static string parseHourly(const string&);
};

#endif  // UTILITYHOURLYOLDAPI_H
