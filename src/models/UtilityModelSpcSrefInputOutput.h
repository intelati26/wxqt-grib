// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELSPCSREFINPUTOUTPUT_H
#define UTILITYMODELSPCSREFINPUTOUTPUT_H

#include "ObjectModel.h"
#include "RunTimeData.h"

class UtilityModelSpcSrefInputOutput {
public:
    static RunTimeData getRunTime();
    static string getImageUrl(ObjectModel *);
    static const string srefPattern2;
    static const string srefPattern3;
};

#endif  // UTILITYMODELSPCSREFINPUTOUTPUT_H
