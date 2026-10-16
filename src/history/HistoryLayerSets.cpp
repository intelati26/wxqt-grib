// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/HistoryLayerSets.h"

const std::vector<HistoryLayerSets::Set>& HistoryLayerSets::all() {
    static const std::vector<Set> sets{
        {"storm-day", "Storm day: GOES infrared with the radar over it", {"satellite/goes-history", "radar/mrms"}, 500.0},
        {"radar", "Radar only (MRMS)", {"radar/mrms"}, 300.0},
        {"satellite-day", "Satellite, the day's true color pictures (NASA GIBS)", {"satellite/polar"}, 900.0},
        {"tropical", "Tropical cyclone: GOES, the NHC cones against the track, the outlook areas", {"satellite/goes-history", "tropical/cone-history", "tropical/outlook-history"}, 1500.0},
        {"climate", "Climate: how far from normal the days before were (ACIS)", {"climate/acis-grid"}, 1800.0},
        {"all-time", "Everything with a time bar: GOES, radar, cones, outlooks", {"satellite/goes-history", "radar/mrms", "tropical/cone-history", "tropical/outlook-history"}, 1200.0},
    };
    return sets;
}

int HistoryLayerSets::defaultFor(const HistoryEvent& event) {
    const auto& source = event.source;
    if (source == "storm") {
        return 3;
    }
    if (source == "climate-day") {
        return 4;
    }
    return 0;   // outbreaks, High risk days and the person's own events: the weather of the day
}
