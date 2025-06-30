// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "radar/Fronts.h"
#include "objects/Color.h"
#include "radar/Projection.h"
#include "util/UtilityList.h"

Fronts::Fronts(FrontTypeEnum type)
    : type{type}
{
    switch (type) {
        case COLD:
            penColor = Color::BLUE_PEN;
            break;
        case WARM:
            penColor = Color::RED_PEN;
            break;
        case STNRY:
            penColor = Color::BLUE_PEN;
            break;
        case STNRY_WARM:
            penColor = Color::RED_PEN;
            break;
        case OCFNT:
            penColor = Color::PINK_PEN;
            break;
        case TROF:
            penColor = Color::BROWN_PEN;
            break;
    }
}

void Fronts::translate(int paneIndex, const ProjectionNumbers& pn) {
    if (coordinates.size() > 3) {
        coordinatesModified[paneIndex].clear();
        for (auto i : range3(0, coordinates.size(), 2)) {
            if (i + 1 < coordinates.size()) {
                const auto coords1 = Projection::computeMercatorNumbersFromLatLon(coordinates[i], pn);
                const auto coords2 = Projection::computeMercatorNumbersFromLatLon(coordinates[i + 1], pn);
                coordinatesModified[paneIndex].push_back(QLineF{coords1[0], coords1[1], coords2[0], coords2[1]});
            }
        }
    }
}
