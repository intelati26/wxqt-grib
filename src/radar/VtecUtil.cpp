// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "VtecUtil.h"
#include "objects/ObjectDateTime.h"
#include "util/UtilityString.h"

bool VtecUtil::isVtecCurrent(const string& vtec) {
    // example "190512T1252Z-190512T1545Z"
    const auto vtecTimeRange = UtilityString::parse(vtec, "-([0-9]{6}T[0-9]{4})Z");
    const auto vtecTime = ObjectDateTime::decodeVtecTime(vtecTimeRange);
    const auto currentTime = ObjectDateTime::decodeVtecTime(ObjectDateTime::getGmtTimeForVtec());
    return currentTime.isBefore(vtecTime);
}
