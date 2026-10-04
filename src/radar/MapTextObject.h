// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MAPTEXTOBJECT_H
#define MAPTEXTOBJECT_H

#include <string>
#include <vector>
#include "objects/FileStorage.h"
#include "radar/MapState.h"
#include "ui/TextViewMetal.h"

using std::string;
using std::vector;

class MapTextObject {
public:
    MapTextObject(int, MapState *, FileStorage *);
    void initialize();
    void add();
    void addWpcPressureCenters();
    void addTextLabelsObservations();

private:
    void initializeTextLabelsCitiesExtended() const;
    void checkAndDrawText(vector<TextViewMetal>&, double, double, const string&, bool);
    static void initializeTextLabelsCountyLabels();
    void addTextLabelsCitiesExtended();
    void addTextLabelsCountyLabels();
    MapState * mapState;
    FileStorage * fileStorage;
    size_t maxCitiesPerGlview;
    static const double cityMinZoom;
    static const double obsMinZoom;
    static const double countyMinZoom;
};

#endif  // MAPTEXTOBJECT_H
