// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WORLDRENDER_H
#define WORLDRENDER_H

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <QDateTime>
#include <QImage>
#include "gfs/GfsChart.h"
#include "gfs/GfsGrid.h"
#include "gfs/GfsRender.h"

// The model charts as grids on the world: the chart's fill of every model that has it (the coarse global ones first, the fine regional ones over them, as the combo map does) and the world painted from
// those grids themselves, at their own resolution, on any of the export projections (the globe, Robinson, Equal Earth, Mollweide, the poles). Used by the master map's high resolution export. (The
// globe on the screen is the master map's own globe mode, GlobeMapView.)
namespace WorldRender {
    struct Source {
        std::string model;
        double km{0};
        GfsGrid::Grid grid;
        GfsGrid::Grid windU, windV;   // m/s, when the chart has barbs
        std::vector<GfsChart::ContourOut> contours;   // the chart's lines (heights ...)
        GfsChart::Ramp ramp;
        std::string run;
        int hour{0};
    };

    // the fill grid of each model for the valid time (its own run and hour), coarse to fine; blocking: call it from a thread
    std::vector<Source> loadSources(const std::string& chart, const std::vector<std::string>& names, const QDateTime& when);
    // the world on one of the export projections painted from the grids themselves, at their own resolution (any width): each pixel samples the finest model that has a value; coastlines and
    // borders over it. Threads work on bands of rows; `done` counts the rows made.
    QImage renderWorld(const std::vector<Source>& sources, int projection, double centerLon, int width, std::atomic<int> * done);
}

#endif  // WORLDRENDER_H
