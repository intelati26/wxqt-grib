// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CitiesExtended.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"

vector<CityExt> CitiesExtended::cities;

void CitiesExtended::create() {
    if (cities.empty()) {
        const auto text = UtilityIO::readTextFile(GlobalVariables::resDir + "cityall.txt");
        const auto lines = WString::split(text, GlobalVariables::newline);
        for (const auto& line : lines) {
            const auto items = WString::split(line, ",");
            if (items.size() > 2) {
                // cityall.txt's longitude column is already correctly
                // negative (US-only cities) - this used to negate it AGAIN,
                // flipping every city to the wrong (positive/eastern-
                // hemisphere) side of the globe. Pre-existing bug, only
                // surfaced now that something (drawCityLabels) actually
                // bbox-filters on real coordinates instead of just plotting
                // whatever position Nexrad's own Mercator projection gave it.
                const auto population = items.size() > 3 ? To::int64(items[3]) : 0;
                cities.emplace_back(items[0], To::Double(items[1]), To::Double(items[2]), population);
            }
        }
    }
}
