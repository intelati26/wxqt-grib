// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardHazards.h"
#include "misc/AlertsDetail.h"
#include "util/UtilityString.h"

CardHazards::CardHazards(Window * parent, const Hazards& objectHazards) {
    const auto ids = UtilityString::parseColumn(objectHazards.data, "\"id\": \"(http.*?)\"");
    const auto hazards = UtilityString::parseColumn(objectHazards.data, "\"event\": \"(.*?)\"");
    auto index = 0;
    for (const auto& hazard : hazards) {
        labels.emplace_back(parent, None, hazard);
        const auto url = ids[index];
        labels.back().connect([url, parent] { new AlertsDetail{parent, url}; });
        box.addWidget(labels.back());
        index += 1;
    }
    addLayout(box);
}

void CardHazards::removeLabels() {
    box.removeChildren();
}
