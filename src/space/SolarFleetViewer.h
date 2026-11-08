// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOLARFLEETVIEWER_H
#define SOLARFLEETVIEWER_H

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include "ui/VBox.h"
#include "ui/Window.h"

class FleetCanvas;

// Who is watching the Sun: a plan view of the inner solar system from above the ecliptic (the Sun, the orbits of Mercury, Venus and the Earth, and where STEREO-A, Parker Solar Probe and Solar Orbiter
// are now, from JPL Horizons, with a month of track and the magnetic field line each is on), and a strip of the space near the Earth (the satellites at L1, geosynchronous orbit and low orbit). Each
// spacecraft has its instruments listed, and the ones whose pictures the program shows open them.
class SolarFleetViewer : public Window {
public:
    explicit SolarFleetViewer(Window * parent);

private:
    void load();
    void select(int row);
    void closeEventCustom() override { closed = true; }
    VBox box;
    QLabel * status{};
    FleetCanvas * canvas{};
    QListWidget * list{};
    QLabel * detail{};
    QWidget * links{};
    QVBoxLayout * linksLayout{};
    int generation{0};
    bool closed{false};
};

#endif  // SOLARFLEETVIEWER_H
