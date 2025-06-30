// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "StormReport.h"

StormReport::StormReport(
    const string& text,
    const string& lat,
    const string& lon,
    const string& time,
    const string& magnitude,
    const string& address,
    const string& city,
    const string& state,
    const string& damageReport,
    const string& damageHeader
)
    : text{text}
    , lat{lat}
    , lon{lon}
    , time{time}
    , magnitude{magnitude}
    , address{address}
    , city{city}
    , state{state}
    , damageReport{damageReport}
    , damageHeader{damageHeader}
    , latLon{lat, lon}
{}
