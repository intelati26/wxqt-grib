// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NhcStormDetails.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityMath.h"
#include "util/UtilityString.h"

NhcStormDetails::NhcStormDetails(
    const string& name,
    const string& movementDir,
    const string& movementSpeed,
    const string& pressure,
    const string& binNumber,
    const string& stormId,
    [[maybe_unused]] const string& lastUpdate,
    const string& classification,
    const string& lat,
    const string& lon,
    const string& intensity,
    const string& status,
    const string& advisoryUrl)
    : name{name}
    , movementDir{movementDir}
    , movementSpeed{movementSpeed}
    , pressure{pressure}
    , binNumber{binNumber}
    , stormId{stormId}
    , classification{classification}
    , lat{lat}
    , lon{lon}
    , intensity{intensity}
    , status{status}
    , advisoryUrl{advisoryUrl}
    , center{lat + " " + lon}
    , goesUrl{"https://cdn.star.nesdis.noaa.gov/FLOATER/data/" + WString::toUpper(stormId) + "/GEOCOLOR/latest.jpg"}
    , movement{UtilityMath::bearingToDirection(To::Int(movementDir)) + "(" + movementDir + ") at " + movementSpeed + " mph"}
    , modBinNumber{WString::replace(WString::toUpper(UtilityString::substring(stormId, 0, 4)), "AL", "AT")}
    , baseUrl{"https://www.nhc.noaa.gov/storm_graphics/" + modBinNumber + "/" + WString::toUpper(stormId)}
    , coneBytes{UtilityIO::downloadAsByteArray(baseUrl + "_5day_cone_with_line_and_wind_sm2.png")}
    , advisoryNumber{WString::replace(WString::split(advisoryUrl, "/").back(), ".shtml", "")}
{}

string NhcStormDetails::forTopHeader() const {
    return movement + ", " + pressure + " mb, " + intensity + " mph";
}
