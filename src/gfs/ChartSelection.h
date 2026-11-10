// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTSELECTION_H
#define CHARTSELECTION_H

#include <string>
#include <vector>
#include <QDateTime>
#include "gfs/GfsRender.h"

// What chart is on show, wherever it is shown: the model, the chart, the lines and barbs ticked onto it, the other models drawn over it, and how it is varied (one member, standard
// deviations, the change since an older run, the largest of a day). The Model Viewer and the master map's model layer each hold these; this is the one form of them that either can give
// and take, so that a chart handed from one screen to the other arrives whole, and so that both make the same request of the renderer from it.
struct ChartSelection {
    std::string model{"GFS"};
    std::string param{"500_wnd_ht"};
    std::vector<std::string> overlays;        // lines and barbs ticked onto the chart
    std::vector<std::string> overlayModels;   // other models whose lines are drawn over this one's, for the same valid time
    std::string member;                       // one ensemble member instead of the mean ("" the chart as it is)
    bool sigma{false};                        // in standard deviations from the normal
    int changeBack{0};                        // hours: the change since the run that much older (0: the plain chart)
    bool maxDay{false};                       // the largest of the fill over the 24 hours ending at the hour shown
    // where and when it is looked at; either may be empty (the screen that takes the selection then keeps its own)
    std::string sector;                       // an area's id (GfsChart::sector or a local one)
    QDateTime valid;                          // the valid time on show, UTC

    // true when the model has that chart
    bool usable() const;
    // the chart and its variants as one line ("model|chart|lines|models|member|sd|change|max": what a saved view keeps) and back; false, and `out` untouched, when the line is not one
    std::string line() const;
    static bool fromLine(const std::string& line, ChartSelection& out);
    // what the renderer is asked for at a forecast hour of the run whose hours are `hours`
    GfsRender::Variant variant(const std::vector<int>& hours, int hour) const;

    static std::string joinList(const std::vector<std::string>& list);
    static std::vector<std::string> splitList(const std::string& text);
};

#endif  // CHARTSELECTION_H
