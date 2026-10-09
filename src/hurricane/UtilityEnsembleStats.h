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
    // How many perturbed members pass within `radiusKm` of a point while at least `minWindKt` strong (the track is followed between the 6-hourly positions by
    // straight lines, the wind likewise); the hours at which those members are nearest. Members that are not a cyclone any more do not count.
    struct Strike {
        int members{0};
        int hits{0};
        double earliest{missing};        // forecast hour of the first member's nearest approach (of those that hit)
        double medianHour{missing};
        double latest{missing};
        double share() const { return members > 0 ? static_cast<double>(hits) / members : 0.0; }
    };
    static Strike strike(const UtilityEcmwfTracks::Storm&, double lat, double lon, double radiusKm, double minWindKt);
    // The share of members that strike each cell of a grid (cell centres at south + (row + 0.5) * step, west + (col + 0.5) * step), row-major from the south, covering every
    // position of every member plus the radius; empty when no member has a position.
    struct Field {
        double south{0.0}, west{0.0}, step{0.5};
        int rows{0}, cols{0};
        vector<float> share;
    };
    static Field strikeField(const UtilityEcmwfTracks::Storm&, double radiusKm, double minWindKt, double step = 0.5);
    static double percentile(vector<double> values, double fraction);   // linear between ranks; missing for an empty set
    static double kilometers(double lat1, double lon1, double lat2, double lon2);
};

#endif  // UTILITYENSEMBLESTATS_H
