// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef USERCHARTS_H
#define USERCHARTS_H

#include <string>
#include <vector>
#include <QByteArray>
#include "gfs/GfsChart.h"

// Charts as data (section 21 of docs/plans.md): a file `<name>.chart.json` in the folder of the person's own models describes a chart of one model: an area fill from a field, lines (heights,
// pressure ...) from others, barbs, a color scale by name. It appears in the Model Viewer under the model, in the group "Your charts". See docs/chart-files.md for the format.
//
//   { "version": 1, "charts": [ {
//       "id": "newsurface1", "model": "GFS", "label": "NewSurface1: 2 m temperature and 500 mb height",
//       "fill": { "field": "TMP@2 m above ground", "ramp": "temperature", "title": "Temperature", "quantity": "temperature", "legend_step": 5 },
//       "contours": [ { "field": "HGT@500 mb", "scale": 0.1, "interval": 6, "title": "Height (dam)", "color": "#202020" } ],
//       "barbs": { "u": "UGRD@10 m above ground", "v": "VGRD@10 m above ground" } } ] }
//
// A field is "VARIABLE@level" (NOAA's names, as in the model files: "TMP@850 mb", "PRMSL@mean sea level"), optionally "|detail"; the fill may instead be {"speed": [u, v]}, {"diff": [a, b]},
// {"mean": [a, b]}, with an optional "scale" and "offset" (value * scale + offset). A model that does not have a field draws the chart with a message that says which.
void userChartsRecordProblems(const std::vector<std::string>& problems);   // (for GfsChart::products())

namespace UserCharts {
    // The charts of one file's text; false, with the reason (and the chart it is in), when the file is not valid: nothing of it is used.
    bool parse(const QByteArray& json, std::vector<GfsChart::Product>& out, std::string& why);
    // Every *.chart.json of a folder: the charts, and a problem for each file (or chart) that was left out.
    void load(const std::string& folder, std::vector<GfsChart::Product>& out, std::vector<std::string>& problems);
    // The problems found when the registry's charts were made (the files left out and why), for Settings and Help.
    const std::vector<std::string>& problems();
    // The names a "ramp" may have.
    std::vector<std::string> rampNames();
    // The folder: the person's own models folder (`GfsModels::setUserFolder`).
    std::string folder();
}

#endif  // USERCHARTS_H
