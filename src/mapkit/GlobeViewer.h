// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GLOBEVIEWER_H
#define GLOBEVIEWER_H

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
        GfsChart::Ramp ramp;
        std::string run;
        int hour{0};
    };

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
