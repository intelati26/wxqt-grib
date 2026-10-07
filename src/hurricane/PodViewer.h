// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PODVIEWER_H
#define PODVIEWER_H

#include <memory>
#include <QString>
#include "hurricane/HurricaneData.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// NHC's Tropical Cyclone Plan of the Day as tables: for each suspect area or storm, the planned and requested reconnaissance flights (aircraft, fix
// time, departure, time on station, altitude, target position, type of mission), then the outlook and remarks as written.
class PodViewer : public Window {
public:
    PodViewer(Window * parent, const std::shared_ptr<HurricaneData::PodData>& pod);
    static QString html(const HurricaneData::PodData& pod);
    static QString summary(const HurricaneData::PodData& pod);   // "Plan of the Day 26-128: 6 flights for AL92"

private:
    VBox box;
};

#endif  // PODVIEWER_H
