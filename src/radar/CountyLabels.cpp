// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CountyLabels.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"

vector<string> CountyLabels::names;
vector<LatLon> CountyLabels::location;

void CountyLabels::create() {
    if (names.empty()) {
        const auto text = UtilityIO::readTextFile(GlobalVariables::resDir + "gaz_counties_national.txt");
        const auto lines = WString::split(text, GlobalVariables::newline);
        for (const auto& line : lines) {
            const auto items = WString::split(line, ",");
            if (items.size() < 4) {
                continue;
            }
            names.push_back(items[1]);
            location.emplace_back(To::Double(items[2]), -1.0 * To::Double(items[3]));
        }
    }
}
