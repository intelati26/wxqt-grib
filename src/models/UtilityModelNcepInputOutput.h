// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELNCEPINPUTOUTPUT_H
#define UTILITYMODELNCEPINPUTOUTPUT_H

#include "ObjectModel.h"
#include "RunTimeData.h"

class UtilityModelNcepInputOutput {
public:
    static RunTimeData getRunTime(ObjectModel *);
    static string getImageUrl(ObjectModel *);
    static const string pattern;
};

#endif  // UTILITYMODELNCEPINPUTOUTPUT_H
