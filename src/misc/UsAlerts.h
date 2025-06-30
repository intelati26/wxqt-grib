// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef USALERTS_H
#define USALERTS_H

#include <string>
#include <unordered_map>
#include <vector>
#include "CapAlertXml.h"
#include "ui/ComboBox.h"
#include "ui/CardAlertDetail.h"
#include "ui/Photo.h"
#include "ui/ScrolledWindow.h"
#include "ui/Window.h"
#include "ui/VBox.h"

using std::string;
using std::unordered_map;
using std::vector;

class UsAlerts : public Window {
public:
    explicit UsAlerts(Window *);

private:
    void update();
    void filterEvents();
    VBox box;
    VBox boxText;
    ScrolledWindow sw;
    Photo photo;
    ComboBox comboBox;
    vector<CapAlertXml> capAlerts;
    const vector<string> defaultFilter{"Tornado Warning", "Severe Thunderstorm Warning", "Flash Flood Warning"};
    vector<string> eventList;
    vector<CardAlertDetail> alertCards;
    unordered_map<string, int> filterCount;
    string html;
};

#endif  // USALERTS_H
