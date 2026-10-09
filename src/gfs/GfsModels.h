// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSMODELS_H
#define GFSMODELS_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "gfs/GfsData.h"

// The models drawn from GRIB, each described once. To add a model:
//   1. its Source in GfsData.cpp (the files, the cycle timing, a warp if the grid is not latitude / longitude)
//   2. its entry in the table in GfsModels.cpp (label, sectors, forecast hours, run style, whether it clones the GFS charts and which fields it has)
//   3. its own charts, if it has any the GFS does not, in GfsChart.cpp (one function, listed in the map `own` there)
//   4. run tests/gfs/run_models.sh: it fails when a model is described incompletely
// The chart code, the renderer, the model screen and the tests all read this table and name no model themselves. (The models screen still lists the model guidance site's chart names for
// the GFS and the blend until that site goes: `recipeList` is false for them.)
namespace GfsModels {
    // forecast hours in steps: from, to (inclusive), every `step`
    struct Hours {
        int from, to, step;
    };
    // how a model's charts are made from the GFS ones: the same chart pointed at this model's fields
    struct Clone {
        bool enabled{false};
        std::vector<std::string> ids;                  // only these GFS charts (empty: every chart whose fields the model has)
        std::set<std::string> variables;               // the fields the model has (empty: not checked)
        std::set<std::string> levels;                  // the pressure levels it has (empty: not checked)
        std::vector<std::string> skip;                 // a chart whose id holds any of these is not made
        bool specificHumidity{false};                  // relative humidity has to be worked out from specific humidity
        bool pieces{true};                             // precipitation comes as 6 hour amounts, not running totals
        std::map<std::string, std::string> rename;     // the GFS field name -> the model's own
        std::string labelPrefix;                       // "Mean " for an ensemble mean
        std::vector<std::string> plain;                // charts whose id starts with one of these are made from the GFS chart without the lines MAG draws on it (an ensemble's precipitation has its spread instead)
    };
    enum class Sectors { World, Conus, Storm };       // the regions of the sector list: everywhere, the contiguous United States only, or the storm's own grid
    enum class Missing { Nothing, MainRun, PreviousCycle, PreviousRun };   // when a run lacks a field: nothing, the newest 00 / 06 / 12 / 18Z run, the cycle before six hours further on, or (a run still being posted) one of the last few runs before it
    struct Def {
        std::string id;                                // the model screen's name: "GFS", "RRFS"
        std::string label;                             // the credit under a chart
        std::function<GfsData::Source(const std::string& storm)> source;   // storm only means anything to a model that follows one
        Sectors sectors{Sectors::World};
        std::vector<Hours> hours;                      // the forecast hours the model screen offers
        bool hourlyRuns{false};                        // a run every hour in the run list, not four a day
        bool storm{false};                             // the model is run for a storm: its screen picks the storm, and its grid is the storm's
        bool overlays{false};                          // the GFS set of lines and wind barbs can be ticked onto its charts (a model may have overlays of its own besides)
        bool recipeList{false};                        // the model screen lists the charts in the registry (false: the older list kept from the model guidance site)
        Missing onMissing{Missing::Nothing};
        Clone clone;
    };
    // in the order they are listed and their charts made
    const std::vector<Def>& all();
    const Def * find(const std::string& id);
    // true for a model drawn from GRIB (including the storm model)
    bool draws(const std::string& id);
    // the models whose charts can have lines and barbs ticked onto them
    std::vector<std::string> overlayModels();
}

#endif  // GFSMODELS_H
