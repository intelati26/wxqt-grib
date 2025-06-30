// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELNSSLWRFINPUTOUTPUT_H
#define UTILITYMODELNSSLWRFINPUTOUTPUT_H

#include "ObjectModel.h"
#include "RunTimeData.h"

class UtilityModelNsslWrfInputOutput {
public:
    static RunTimeData getRunTime();
    static string getImageUrl(ObjectModel *);

private:
    static const string baseUrl;
};

#endif  // UTILITYMODELNSSLWRFINPUTOUTPUT_H
