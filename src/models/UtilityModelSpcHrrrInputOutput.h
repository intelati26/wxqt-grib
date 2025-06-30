// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELSPCHRRRINPUTOUTPUT_H
#define UTILITYMODELSPCHRRRINPUTOUTPUT_H

#include "ObjectModel.h"
#include "RunTimeData.h"

class UtilityModelSpcHrrrInputOutput {
public:
    static RunTimeData getRunTime();
    static string getImageUrl(ObjectModel *);
    static string getSectorCode(const string&);
    static string getValidTime(const string&, const string&, const string&);

private:
    static string formatTime(const string&);
};

#endif  // UTILITYMODELSPCHRRRINPUTOUTPUT_H
