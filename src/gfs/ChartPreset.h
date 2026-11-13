// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTPRESET_H
#define CHARTPRESET_H

#include <string>
#include <vector>
#include "gfs/ChartSelection.h"

// A named set of checks (docs/plans.md section 9): a base chart and the lines and barbs ticked onto it. Unlike a saved view it is no model's and no area's: it is the chart as its checks
// (ChartAlias), so it can be put on any model that has the fields. Kept in the preferences, one per line ("name|chart|overlay,overlay"). No picker shows them yet.
namespace ChartPreset {

struct Preset {
    std::string name;
    std::string chart;                  // the base chart's id (read as checks through ChartAlias)
    std::vector<std::string> overlays;  // the lines and barbs ticked onto it (the ids GfsChart::overlayChoices gives)
};

// the preset as one line, and back; false, and `out` untouched, when the line is not one
std::string line(const Preset& preset);
bool fromLine(const std::string& text, Preset& out);
// a name that is safe to keep (no separators or line breaks, no leading or trailing blanks)
std::string cleanName(const std::string& name);
// the preset for the chart on show
Preset fromSelection(const std::string& name, const ChartSelection& selection);

// the presets that come with the program (GFS's charts and lines; on another model `resolve` keeps what it offers): combinations that are not a chart of the list as it is
const std::vector<Preset>& starters();

// the presets kept, in the order they were made; put adds one or replaces the one of that name, erase deletes by name (not called remove: that is also the C function that deletes a file) (false when there is none)
std::vector<Preset> load();
void put(const Preset& preset);
bool erase(const std::string& name);

// A preset on a model: the chart (the same id when the model has it, else the model's one chart with the same checks, else none), and the overlays the model offers (the rest are named in
// `dropped`, for the picker to grey). `found` is false when the model has no such chart.
struct Resolved {
    bool found{false};
    ChartSelection selection;
    std::vector<std::string> dropped;
    bool otherId{false};   // the chart is the model's own id for the same checks, not the preset's id
};
Resolved resolve(const Preset& preset, const std::string& model);

}  // namespace ChartPreset

#endif  // CHARTPRESET_H
