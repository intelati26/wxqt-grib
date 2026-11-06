// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCCOMPMAP_H
#define SPCCOMPMAP_H

#include <map>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include "misc/OutlookNav.h"
#include "ui/ControlRow.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// The SPC's composite map as its page has it: a base map and a layer for each product (satellite, pressure, temperature, dew point, CAPE, ... fronts), each layer a picture with transparent
// areas, any of them ticked on and off; a layer ticked is drawn over the ones before it (the page's own rule: the last one clicked is on top). The layers ticked are kept.
class SpcCompMap : public Window {
public:
    explicit SpcCompMap(Window *);

private:
    OutlookNav nav;
    void reload();                         // fetch every layer that is ticked and not here yet
    void compose();                        // the base map and the ticked layers, in the order ticked, on white
    void layerToggled(const string& id, bool on);
    void saveLayers() const;
    void fetchTimes();                     // what the page says the layers are valid at
    const string prefToken{"SPCCOMPMAP_LAYERS"};
    VBox box;
    ControlRow boxH;
    Photo photo;
    QLabel * valid{};
    QPushButton * refreshButton{};
    std::vector<string> ticked;            // the ids of the layers in the order ticked (the last is on top)
    std::map<string, QImage> pictures;     // the layers fetched, by id ("basemap" too)
    std::map<string, QCheckBox *> boxes;
};

#endif  // SPCCOMPMAP_H
