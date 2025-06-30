// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELWPCGEFSINPUTOUTPUT_H
#define UTILITYMODELWPCGEFSINPUTOUTPUT_H

#include "ObjectModel.h"
#include "RunTimeData.h"

class UtilityModelWpcGefsInputOutput {
public:
    static RunTimeData getRunTime();
    static string getImageUrl(ObjectModel *);
};

#endif  // UTILITYMODELWPCGEFSINPUTOUTPUT_H
