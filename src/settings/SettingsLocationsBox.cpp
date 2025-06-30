// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SettingsLocationsBox.h"
#include "settings/Location.h"
#include "settings/ObjectLocation.h"
#include "util/UtilityList.h"

SettingsLocationsBox::SettingsLocationsBox(Window * parent)
    : Widget{parent}
    , parent{parent}
{
    addLocations();
    setLayout(box.getView());
}

void SettingsLocationsBox::refresh() {
    box.removeChildren();
    Location::refresh();
    addLocations();
    Location::setMainScreenComboBox();
}

void SettingsLocationsBox::addLocations() {
    buttons.clear();
    locationCards.clear();
    hboxList.clear();
    for (auto index : range(Location::getNumLocations())) {
        hboxList.emplace_back();

        buttons.emplace_back(parent, Down, "Move down");
        buttons.back().connect([this, index] { moveDownClicked(index); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, Up, "Move up");
        buttons.back().connect([this, index] { moveUpClicked(index); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, Delete, "Delete");
        buttons.back().connect([this, index] { deleteClicked(index); });
        hboxList.back().addWidget(buttons.back());

        locationCards.emplace_back(parent, index);
        hboxList.back().addLayout(locationCards.back());
        box.addLayout(hboxList.back());
    }
    box.addStretch();
}

void SettingsLocationsBox::deleteClicked(int locationIndex) {
    if (Location::getNumLocations() > 1) {
        Location::deleteLocation(locationIndex);
        refresh();
    }
}

void SettingsLocationsBox::moveDownClicked(int position) {
    if (position < (Location::getNumLocations() - 1)) {
        auto locA = ObjectLocation{position};
        auto locB = ObjectLocation{position + 1};
        locA.saveToNewSlot(position + 1);
        locB.saveToNewSlot(position);
    } else {
        auto locA = ObjectLocation{position};
        auto locB = ObjectLocation{0};
        locA.saveToNewSlot(0);
        locB.saveToNewSlot(position);
    }
    refresh();
}

void SettingsLocationsBox::moveUpClicked(int position) {
    if (position > 0) {
        auto locA = ObjectLocation{position - 1};
        auto locB = ObjectLocation{position};
        locA.saveToNewSlot(position);
        locB.saveToNewSlot(position - 1);
    } else {
        auto locA = ObjectLocation{Location::getNumLocations() - 1};
        auto locB = ObjectLocation{0};
        locA.saveToNewSlot(0);
        locB.saveToNewSlot(Location::getNumLocations() - 1);
    }
    refresh();
}
