// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYACEFORECAST_H
#define UTILITYACEFORECAST_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

// The accumulated cyclone energy (ACE) a storm of the Weather Lab large ensemble (FNV3, 1000 members) makes, member by member and hour by hour: ACE is 10^-4 of the sum of the squares of the
// maximum sustained wind (knots) at every 6 hourly step at 34 knots or more. A storm is one track of the cyclogenesis file: a storm that exists (its ATCF id) or a possible one (a number) that
// some of the members form. The statistics are of the members that form it ("conditional on formation") and of all of them, a member that does not form having none ("weighted").
// Pure text and arithmetic, no network.
namespace UtilityAceForecast {
    struct Member {
        std::vector<int> hours;      // lead hours, ascending
        std::vector<float> wind;     // knots
    };
    struct Storm {
        std::string track;                // "AL102026" or "7"
        std::map<int, Member> members;    // by the file's sample number: the members that have this storm
    };
    bool parse(std::string_view csv, std::vector<Storm>& storms, int& ensembleSize, std::string& cycle, std::string& error);

    struct Summary {
        int ensembleSize{0};
        int formed{0};
        double probability{0.0};          // formed / ensembleSize
        std::vector<int> hours;           // 0, 6, 12 ... the steps of the lines
        // by step, over the members that form the storm
        std::vector<double> mean, median, p10, p25, p75, p90, minimum, maximum;
        std::vector<double> producedMean;               // the mean ACE made in the 6 hours ending at the step
        // by step, over all the members (the ones that do not form count as no ACE)
        std::vector<double> weightedMean, weightedMedian;
        std::vector<std::vector<double>> cumulative;    // [member that forms][step]: the running total
        std::vector<double> lifetime;                   // each such member's total at the end of its track
    };
    Summary summarize(const Storm& storm, int ensembleSize);

    // the numbers on the tiles for one step
    struct Tiles {
        double maximum{0}, p90{0}, p75{0}, mean{0}, median{0}, weightedMean{0}, weightedMedian{0}, p25{0}, p10{0}, minimum{0};
        bool valid{false};
    };
    Tiles tilesAt(const Summary& summary, size_t step);
    std::vector<double> valuesAt(const Summary& summary, size_t step);   // the members' running totals at a step (for the histogram)
    double percentile(std::vector<double> values, double fraction);       // 0..1, linear between the ranks; 0 for none
}

#endif  // UTILITYACEFORECAST_H
