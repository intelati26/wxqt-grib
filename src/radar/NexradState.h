// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADSTATE_H
#define NEXRADSTATE_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "radar/ProjectionNumbers.h"
#include "ui/TextViewMetal.h"

using std::string;
using std::vector;

// The state of a map: its centre (the projection is built round a radar site's position), pan, zoom and the text labels drawn on it.
class NexradState {
public:
    NexradState(int, int, bool, const string&, int, int);
    void reset();
    ProjectionNumbers getPn() const;
    string getRadarSite() const;
    void setRadar(const string&);
    int paneNumber;
    int numberOfPanes;
    bool useASpecificRadar;
    double xPos{};
    double yPos{};
    double zoom{1.0};
    vector<TextViewMetal> cities;
    vector<TextViewMetal> countyLabels;
    vector<TextViewMetal> pressureCenterLabelsRed;
    vector<TextViewMetal> pressureCenterLabelsBlue;
    vector<TextViewMetal> observations;
    double zoomToHideMiscFeatures{0.2};
    // the map's centre site and its projection are read by download threads and may be changed by the UI
    std::shared_ptr<std::recursive_mutex> lock{std::make_shared<std::recursive_mutex>()};

private:
    ProjectionNumbers pn;
    string radarSite;
public: // TODO FIXME
    int originalWidth{};
    int originalHeight{};
};

#endif  // NEXRADSTATE_H
