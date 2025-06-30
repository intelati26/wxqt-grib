// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityWpcRainfallOutlook.h"

const vector<string> UtilityWpcRainfallOutlook::codes{
    "QPF94E",
    "QPF98E",
    "QPF99E",
    "PMDEPD",
    "PMDEPD"
};

const vector<string> UtilityWpcRainfallOutlook::urls{
    "https://www.wpc.ncep.noaa.gov/qpf/94ewbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/98ewbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/99ewbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/ero_d45/images/d4wbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/ero_d45/images/d5wbg.gif"
};
