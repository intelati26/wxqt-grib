// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADSTATE_H
#define NEXRADSTATE_H

#include <cstdint>
#include <string>
#include <vector>
#include "radar/ProjectionNumbers.h"
#include "ui/TextViewMetal.h"

using std::string;
using std::vector;

class NexradState {
public:
    NexradState(int, int, bool, const string&, int, int);
    void reset();
    ProjectionNumbers getPn() const;
    string getRadarSite() const;
    void setRadar(const string&);
    string getRadarProduct() const;
    uint16_t getRadarProductId() const;
    void setRadarProduct(const string&);
    bool isTdwrSite() const;
    bool isTdwrProduct() const;
    void readPreferences();
    void writePreferences() const;
    int paneNumber;
    int numberOfPanes;
    bool useASpecificRadar;
    double xPos{};
    double yPos{};
    double zoom{1.0};
    int tiltInt{};
    vector<TextViewMetal> cities;
    vector<TextViewMetal> countyLabels;
    vector<TextViewMetal> pressureCenterLabelsRed;
    vector<TextViewMetal> pressureCenterLabelsBlue;
    vector<TextViewMetal> observations;
    const vector<string> initialRadarProducts{"N0Q", "N0U", "EET", "DVL"};
    double zoomToHideMiscFeatures{0.2};

private:
    ProjectionNumbers pn;
    string radarSite;
    const string radarType{"WXMETAL"};
    string radarProduct{"N0Q"};
public: // TODO FIXME
    int originalWidth{};
    int originalHeight{};
};

#endif  // NEXRADSTATE_H
