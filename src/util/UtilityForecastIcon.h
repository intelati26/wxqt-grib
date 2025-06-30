// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYFORECASTICON_H
#define UTILITYFORECASTICON_H

#include <string>
#include "ui/Pix.h"

using std::string;

class UtilityForecastIcon {
public:
    static Pix getIcon(const string&);

private:
    static Pix parseBitmapString(const string&);
    static Pix getDualBitmapWithNumbers(const string&, const string&);
    static Pix getBitmapWithOneNumber(const string&);
    static string getFilename(const string&);
};

#endif  // UTILITYFORECASTICON_H
