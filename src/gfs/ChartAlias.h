// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTALIAS_H
#define CHARTALIAS_H

#include <string>
#include <vector>

// Every chart id read as "a family and its settings" (docs/plans.md section 9, "Charts as presets of the checks"): the fill's field and level, the mode (value, chance, change ...), what a chance
// or an ensemble product is of (product | hour | limit | kind), the line families, and whether barbs, streamlines and type bands are on. Saved views, favorites, the comparison tiles and the
// Extended page's hand-off all keep chart ids; this table lets the picker show an id as checks (a chart opens with its own checks ticked) and find the id of a set of checks (the picker lands on a
// real chart). No picker uses it yet. The data is generated (ChartAliasData.cpp).
namespace ChartAlias {

struct Entry {
    const char * id;          // the chart's id
    const char * label;       // its name in the picker today
    const char * category;    // the group it sits in today
    int models;               // how many models have a chart of this id
    const char * family;      // "Value (field at a level)", "Precipitation (period)", "Chance", "Amount (expected, percentile)", "Ensemble (members, spread)", "Anomaly", "Change",
                              // "Product grid (product, hour, type)", or "One-off" (no family reads it: it is its own alias)
    const char * field;       // the fill's field, with its numbers as the chart writes them ("" for lines only)
    const char * level;       // "500 mb", "850-700 mb", "10 m" ("" none)
    const char * mode;        // "Value", "Chance of a limit", "Amount: expected / percentile", "Ensemble: members / spread", "Anomaly", "Change"
    const char * chance;      // "product | hour | limit | kind" of a chance or product-grid chart ("" none)
    const char * product;     // a chance or a product grid chart's parts: what it is of ("Snowfall", "2-5 km Updraft Helicity"), the hour ("24 h", "instant"), the limit ("> 2 in") and the kind
    const char * window;      // ("Ensemble mean", "Paintball", "Neighborhood probability", "ensemble"); empty for every other chart
    const char * limit;
    const char * kind;
    const char * values;      // the numbers in the id, in order, as the picker's dropdown values ("24 | 05": the hour and the percentile); with the rest they name the chart exactly
    const char * contours;    // the line families, comma separated ("Pressure, Thickness")
    bool barbs;
    bool streamlines;
    bool bands;               // the rain / snow / mixed type bands
    const char * group;       // the id with its numbers as '#': charts of one group differ by a number (the picker's dropdown)
    bool single;              // alone in its group
    bool differsByModel;      // the same id is not the same checks on every model (a model-specific alias)
};

// every id, in id order
const std::vector<Entry>& table();
// the entry of an id, or null when there is none
const Entry * find(const std::string& id);
// the id of the entry with these checks (the family, field, level, mode, chance, values, contours, barbs, streamlines and bands of `like`), "" when there is none or more than one
std::string idFor(const Entry& like);
// true when the two entries have the same checks (their ids may differ: the same chart on another model under another id)
bool sameChecks(const Entry& a, const Entry& b);
// how many entries share their checks with another one (0: the checks name a chart exactly)
int ambiguous();
// true when the entry is a one-off: no family reads it, it keeps its own id
bool oneOff(const Entry& e);

}  // namespace ChartAlias

#endif  // CHARTALIAS_H
