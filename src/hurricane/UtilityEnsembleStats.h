// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYENSEMBLESTATS_H
#define UTILITYENSEMBLESTATS_H

#include <vector>
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityEcmwfTracks.h"

using std::vector;

// How the members of an ensemble are spread, hour by hour: the intensity and pressure percentiles, the spread of the positions, and the
// share of members that are still a cyclone or reach a wind threshold. Pure number work.
class UtilityEnsembleStats {
public:
    static constexpr double missing = UtilityEcmwfTracks::missing;
    struct Hour {
        int hour{0};
        int alive{0};                 // members with a position at this hour
        int total{0};                 // members counted
        // percentiles of the members that are alive; missing when none are
        double windMin{missing}, wind10{missing}, wind25{missing}, windMedian{missing}, wind75{missing}, wind90{missing}, windMax{missing};   // kt
        double pressMin{missing}, press10{missing}, press25{missing}, pressMedian{missing}, press75{missing}, press90{missing}, pressMax{missing};   // mb
        double centerLat{missing}, centerLon{missing};   // the mean position
        double radius50{missing}, radius90{missing};     // km from it that hold half / nine tenths of the members that are alive
        double probTs{0.0}, probHurricane{0.0}, probMajor{0.0};   // share of all members at 34 / 64 / 96 kt or more (a member that has gone counts as below)
        double probAlive{0.0};                                    // share of all members that still have a position
    };
    // the perturbed members only (type 4 and the perturbed 2 / 3), hours every `step` hours up to the longest member
    static vector<Hour> compute(const UtilityEcmwfTracks::Storm&, int step = 6);
    // NOAA's GEFS as members (ATCF AP01 .. AP30, control AC00), so the same statistics can be taken; false when the guidance has none
    static bool fromGefs(const vector<UtilityAtcf::Track>& guidance, UtilityEcmwfTracks::Storm& storm);
    static double percentile(vector<double> values, double fraction);   // linear between ranks; missing for an empty set
    static double kilometers(double lat1, double lon1, double lat2, double lon2);
};

#endif  // UTILITYENSEMBLESTATS_H
