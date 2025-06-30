// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef DOWNLOADTEXT_H
#define DOWNLOADTEXT_H

#include <string>

using std::string;

class DownloadText {
public:
    static string byProduct(const string&);
    static string getTextProductWithVersion(const string&, int);

private:
    static bool useNwsApi;
};

#endif  // DOWNLOADTEXT_H
