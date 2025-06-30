// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELESRLINPUTOUTPUT_H
#define UTILITYMODELESRLINPUTOUTPUT_H

#include <string>
#include "ObjectModel.h"
#include "RunTimeData.h"

using std::string;

class UtilityModelEsrlInputOutput {
public:
    static RunTimeData getRunTime(ObjectModel *);
    static string getImageUrl(ObjectModel *);
    static string monthWordToNumber(const string&);
    static const string pattern1;
    static const string pattern2;
    static const string pattern3;
    static const string pattern4;
    static const string pattern5;
    static const string pattern6;
};

#endif  // UTILITYMODELESRLINPUTOUTPUT_H
