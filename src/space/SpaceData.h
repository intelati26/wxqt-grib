// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPACEDATA_H
#define SPACEDATA_H

#include <string>
#include <vector>
#include "space/UtilitySpace.h"

// The downloads behind the space weather screen (these block: call them off the GUI thread). Every feed is read side by side; one that fails is left
// empty and named in `problems`, so the rest of the screen still fills in.
class SpaceData {
public:
    struct Bundle {
        std::vector<UtilitySpace::ScaleDay> scales;
        std::vector<UtilitySpace::Point> kp;       // observed, estimated and predicted, three-hourly
        std::vector<UtilitySpace::Point> xray;     // the last day, 5 minute steps
        std::vector<UtilitySpace::Point> wind;     // speed and density, 5 minute steps
        std::vector<UtilitySpace::Point> mag;      // Bt and Bz
        std::vector<UtilitySpace::Point> proton;   // >= 10 MeV protons (pfu): S1 is 10
        std::vector<UtilitySpace::Point> electron; // >= 2 MeV electrons (pfu): the alert level is 1,000
        std::vector<UtilitySpace::Point> cycle;    // monthly sunspot number and its smoothed value since 1990
        std::vector<UtilitySpace::Band> predicted; // the predicted smoothed sunspot number of the cycle
        UtilitySpace::Flare flare;
        std::vector<std::string> alerts;
        std::string problems;
    };
    static void load(Bundle& bundle);
    static UtilitySpace::Ovation loadOvation();   // the aurora model grid (a 900 kB file)
    static std::string url(const char * path) { return std::string{"https://services.swpc.noaa.gov/"} + path; }
};

#endif  // SPACEDATA_H
