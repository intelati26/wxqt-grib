// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "PressureCenter.h"

PressureCenter::PressureCenter(PressureCenterTypeEnum centerType, const string& pressureInMb, double lat, double lon)
    : centerType{centerType}
    , pressureInMb{pressureInMb}
    , lat{lat}
    , lon{lon}
{}
