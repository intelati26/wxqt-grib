// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SETTINGSLOCATIONSBOX_H
#define SETTINGSLOCATIONSBOX_H

#include <vector>
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/CardLocationItem.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::vector;

class SettingsLocationsBox : public Widget {
public:
    explicit SettingsLocationsBox(Window *);
    void refresh();

private:
    void addLocations();
    void deleteClicked(int);
    void moveDownClicked(int);
    void moveUpClicked(int);
    VBox box;
    Window * parent;
    vector<Button> buttons;
    vector<CardLocationItem> locationCards;
    vector<HBox> hboxList;
};

#endif  // SETTINGSLOCATIONSBOX_H
