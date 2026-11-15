// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The SPC post-processed products of the GEFS (SPCPOST, SPCGEFS).
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

void spcProducts(std::vector<Product>& p, const char * source, bool gefs) {
    const auto add = [&] (const std::string& id, const std::string& label, const std::string& file, const char * stat, const std::string& title) {
        Product x;
        x.source = source;
        x.id = id;
        x.label = label;
        x.wants = {GfsData::Want{"p", "PROB", file, "", "", stat}};
        x.fill = [] (const Grids& g) { return pick(g, "p"); };
        x.ramp = probability();
        x.palette = "prob";
        x.fillTitle = title;
        x.legendStep = 10;
        p.push_back(x);
    };
    if (!gefs) {
        for (const auto& [id, period, text] : {std::tuple<const char *, const char *, const char *>{"1hr", "refsct_1hr", "1-hour"}, {"4hr", "refsct_4hr", "4-hour"}, {"full", "refsct_full", "rest of the convective day"}}) {
            add(std::string{"thunder_"} + id, std::string{"REFSCT Thunderstorm Probability ("} + text + ")", std::string{"refsct/"} + period, "", std::string{"Chance of a thunderstorm, "} + text + " (%)");
        }
        const std::pair<const char *, const char *> kinds[] = {{"tor", "Tornado"}, {"hail", "Hail"}, {"wind", "Wind"}};
        for (int k = 0; k < 3; k++) {
            const std::string band = "b" + std::to_string(k + 1);
            add(std::string{"severe_"} + kinds[k].first + "_4hr", std::string{"GREFS Calibrated "} + kinds[k].second + " Probability (4-hour)", "grefs/grefs_{ee}.4hr;grefs/grefs_{pe}.4hr", band.c_str(),
                std::string{"Chance of "} + kinds[k].first + " severe within 25 miles, 4 hours (%)");
            add(std::string{"severe_"} + kinds[k].first + "_full", std::string{"GREFS Calibrated "} + kinds[k].second + " Probability (rest of the convective day)", "grefs/grefs_{ee}.*hr;grefs/grefs_{pe}.*hr", band.c_str(),
                std::string{"Chance of "} + kinds[k].first + " severe within 25 miles, rest of the convective day (%)");
        }
    } else {
        add("thunder_24hr", "GEFSCT Thunderstorm Probability (24-hour, 12Z to 12Z)", "gefsct/gefsct", "", "Chance of a thunderstorm in the convective day (%)");
        for (const auto& [what, label] : {std::pair<const char *, const char *>{"tor", "Tornado"}, {"wind", "Wind"}, {"hail", "Hail"}, {"severe", "Any Severe"}}) {
            for (const auto& [level, text] : {std::pair<const char *, const char *>{"std", "(Any Severe)"}, {"sig", "(Significant)"}}) {
                add(std::string{"gefsmlp_"} + what + "_" + level, std::string{"GEFS-MLP "} + label + " Probability " + text, std::string{"gefs_mlp/gefsmlp_"} + what + "_" + level, "",
                    std::string{"Chance of "} + what + " severe within 25 miles (%)");
            }
        }
    }
}

}  // namespace detail
}  // namespace GfsChart
