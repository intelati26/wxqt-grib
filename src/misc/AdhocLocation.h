// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef ADHOCLOCATION_H
#define ADHOCLOCATION_H

#include <string>
#include <unordered_map>
#include <vector>
#include "ui/Button.h"
#include "ui/CardHazards.h"
#include "ui/CardCurrentConditions.h"
#include "ui/HBox.h"
#include "ui/SevenDayCollection.h"
#include "ui/ScrolledWindow.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "util/CurrentConditions.h"
#include "util/Hazards.h"
#include "util/SevenDay.h"

using std::string;
using std::unordered_map;
using std::vector;

class AdhocLocation : public Window {
public:
    AdhocLocation(Window *, const LatLon&);

private:
    void reload();
    void getCc();
    void updateCc();
    void update7day();
    void get7day();
    void updateHazards();
    void getHazards();
    VBox box;
    HBox boxH;
    VBox boxCc;
    VBox boxSevenDay;
    VBox boxHazards;
    ScrolledWindow sw;
    LatLon latLon;
    Button button;
    CurrentConditions currentConditions;
    Hazards hazards;
    SevenDay sevenDay;
    CardCurrentConditions cardCurrentConditions;
    SevenDayCollection sevenDayCollection;
    CardHazards cardHazards;
    string locationName;
};

#endif  // ADHOCLOCATION_H
