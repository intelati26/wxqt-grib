// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DAMDATA_H
#define DAMDATA_H

#include <string>
#include <vector>
#include "dams/UtilityDams.h"

// What the screens download from the Corps' CWMS Data API for the dams of UtilityDams (English units: ft, cfs, MWh). These block, so call them off the GUI thread.
class DamData {
public:
    static const vector<UtilityDams::Project>& projects();   // the registry (a resource), read once

    struct Latest {              // the newest hourly values of one dam
        const UtilityDams::Project * project{nullptr};
        double pool{UtilityDams::missing};         // ft
        double tailwater{UtilityDams::missing};    // ft
        double outflow{UtilityDams::missing};      // cfs
        double power{UtilityDams::missing};        // cfs through the turbines
        double inflow{UtilityDams::missing};       // cfs
        double generation{UtilityDams::missing};   // MWh in the last hour
        long seconds{0};                           // the time of the newest value
        bool ok{false};
    };
    struct Data {
        UtilityDams::Series pool, tailwater, outflow, power, inflow, generation;
        string error;
    };
    static void loadLatest(vector<Latest>& all);                                 // every dam, in parallel; cached for ten minutes
    static Latest loadLatestOne(const UtilityDams::Project&, bool full = true);   // full: also the tailwater, turbine flow and inflow
    static void loadProject(const UtilityDams::Project&, int hours, Data& data); // the history of one dam
    static UtilityDams::Series loadSeries(const UtilityDams::Project&, const string& name, int hours);
};

#endif  // DAMDATA_H
