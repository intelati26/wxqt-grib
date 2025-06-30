// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "AdhocLocation.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "settings/Location.h"
#include "settings/UtilityLocation.h"

AdhocLocation::AdhocLocation(Window * parent, const LatLon& latLon)
    : Window{parent}
    , sw{this, box}
    , latLon{latLon}
    , button{this, None, "Save"}
    , cardCurrentConditions{CardCurrentConditions{this, currentConditions}}
    , sevenDayCollection{this, &boxSevenDay, &sevenDay}
    , cardHazards{this, hazards}
    , locationName{latLon.printPretty() + " - " + UtilityLocation::getNearestCity(latLon)}
{
    boxH.addWidget(button);
    button.connect([this, latLon] {
        Location::save(latLon, locationName);
        Location::setMainScreenComboBox();
        setTitle("Saved location: " + locationName);
    });

    box.addLayout(boxH);
    box.addLayout(boxCc);
    boxCc.addLayout(cardCurrentConditions);
    box.addLayout(boxHazards);
    box.addLayout(boxSevenDay);
    box.addStretch();

    reload();
}

void AdhocLocation::updateHazards() {
    cardHazards.removeLabels();
    cardHazards = CardHazards{this, hazards};
    boxHazards.addLayout(cardHazards);
}

void AdhocLocation::getHazards() {
    hazards.process(latLon);
}

void AdhocLocation::update7day() {
    sevenDayCollection.update();
}

void AdhocLocation::get7day() {
    sevenDay.process(latLon);
}

void AdhocLocation::updateCc() {
    cardCurrentConditions.update(currentConditions);
}

void AdhocLocation::getCc() {
    currentConditions.process(latLon, 0);
    currentConditions.timeCheck();
}

void AdhocLocation::reload() {
    setTitle(locationName);
    new FutureVoid{this, [this] { getCc(); }, [this] { updateCc(); }};
    new FutureVoid{this, [this] { getHazards(); }, [this] { updateHazards(); }};
    new FutureVoid{this, [this] { get7day(); }, [this] { update7day(); }};
}
