// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SevereWarning.h"
#include "objects/PolygonWarning.h"
#include "util/To.h"

SevereWarning::SevereWarning(PolygonType type)
    : type{type}
{
    generateString();
}

void SevereWarning::download() {
    PolygonWarning::byType[type]->download();
    generateString();
}

void SevereWarning::generateString() {
    const auto html = PolygonWarning::byType[type]->getData();
    warningList = ObjectWarning::parseJson(html);
}

string SevereWarning::getName() const {
    return PolygonWarning::getLongName(type);
}

string SevereWarning::getShortName() const {
    return PolygonWarning::getShortName(type);
}

string SevereWarning::getCount() const {
    return To::string(getCountAsInt());
}

int SevereWarning::getCountAsInt() const {
    auto i = 0;
    for (const auto& s : warningList) {
        if (s.isCurrent) {
            i += 1;
        }
    }
    return i;
}
