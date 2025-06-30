// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef LOCATIONEDITBOX_H
#define LOCATIONEDITBOX_H

#include <string>
#include <vector>
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/Entry.h"
#include "ui/HBox.h"
#include "ui/Table.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class LocationEditBox : public Widget {
public:
    explicit LocationEditBox(Window *);

private:
    void lookupSearchTerm();
    string getRadarFromCity(const string&);
    void populateLabels(int);
    void blankOutButtons();
    void saveLocation();
    VBox boxResults;
    HBox boxMain;
    Table table;
    Button saveButton;
    Entry cityEdit;
    Entry editName;
    Entry editLat;
    Entry editLon;
    Entry editNexrad;
    vector<Button> buttons;
    vector<string> cities;
};

#endif  // LOCATIONEDITBOX_H
