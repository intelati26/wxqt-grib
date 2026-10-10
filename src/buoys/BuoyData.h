// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef BUOYDATA_H
#define BUOYDATA_H

#include <string>
#include <vector>
#include "buoys/UtilityBuoys.h"

// What the screens download from NDBC: every station's latest observation joined with its name (cached for ten minutes), and one station's recent history.
// These block, so call them off the GUI thread.
class BuoyData {
public:
    struct Marker {
        UtilityBuoys::Station station;
        UtilityBuoys::Obs obs;
        double mercator{0.0};   // for the map
    };
    static bool loadLatest(std::vector<Marker>& markers, std::string& error);
    static bool loadHistory(const std::string& id, std::vector<UtilityBuoys::Obs>& series, std::string& error);   // newest first, as the file has it
};

#endif  // BUOYDATA_H
