// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelSpcHrrrInterface.h"

const vector<string> UtilityModelSpcHrrrInterface::sectors{
    "National",
    "Northwest US",
    "Southwest US",
    "Northern Plains",
    "Central Plains",
    "Southern Plains",
    "Northeast US",
    "Mid Atlantic",
    "Southeast US",
    "Midwest"
};

const vector<string> UtilityModelSpcHrrrInterface::sectorCodes{
    "S19",
    "S11",
    "S12",
    "S13",
    "S14",
    "S15",
    "S16",
    "S17",
    "S18",
    "S20"
};

const vector<string> UtilityModelSpcHrrrInterface::params{
    "refc",
    "pmsl",
    "srh3",
    "cape",
    "proxy",
    "wmax",
    "scp",
    "uh",
    "ptype",
    "ttd"
};

const vector<string> UtilityModelSpcHrrrInterface::labels{
    "Composite Reflectivity",
    "MSL Pressure & Wind",
    "Shear Parameters",
    "Thermo Parameters",
    "Proxy Indicators",
    "Max Surface Wind",
    "SCP / STP",
    "Updraft Helicity",
    "Winter Parameters",
    "Temp/Dwpt"
};
