// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GLOBEVIEWER_H
#define GLOBEVIEWER_H

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include "gfs/GfsChart.h"
#include "gfs/GfsGrid.h"
#include "gfs/GfsRender.h"
#include "ui/Window.h"

class GlobeCanvas;

// The model on a globe: the chart's fill of every model that has it (the coarse global ones first, the fine regional ones over them, as the combo map does) painted on a sphere seen from space,
// with no map distortion: turn it with the mouse, zoom with the wheel, hover for the value of the finest model there, step the valid time by 3 hours. Coastlines and borders are drawn over it.
class GlobeViewer : public Window {
public:
    GlobeViewer(Window * parent, const std::string& param, const std::vector<std::string>& models, const QDateTime& valid, double lat, double lon);

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
    static std::vector<Source> loadSources(const std::string& chart, const std::vector<std::string>& names, const QDateTime& when);
    // the world on one of the export projections painted from the grids themselves, at their own resolution (any width): each pixel samples the finest model that has a value; coastlines and
    // borders over it. Threads work on bands of rows; `done` counts the rows made.
    static QImage renderWorld(const std::vector<Source>& sources, int projection, double centerLon, int width, std::atomic<int> * done);

private:
    void load();
    void closeEventCustom() override { closed = true; }
    std::string param;
    std::vector<std::string> models;
    QDateTime valid;
    std::shared_ptr<std::vector<Source>> sources;
    GlobeCanvas * canvas{};
    QLabel * status{};
    QPushButton * back{};
    QPushButton * forward{};
    int generation{0};
    bool closed{false};
};

#endif  // GLOBEVIEWER_H
