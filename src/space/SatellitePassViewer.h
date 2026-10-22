// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SATELLITEPASSVIEWER_H
#define SATELLITEPASSVIEWER_H

#include <string>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include "space/SatellitePasses.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// When the polar orbiters pass over each tropical storm and over the places of interest (the saved locations and places added here): a schedule of the passes of the next 1 to 3 days with the
// instrument that sees the place (imagery, microwave, scatterometer wind, radar), how far from the middle of its swath, going north or south, and the sun there. The "map" button shows the same
// passes as swaths on the master map.
class SatellitePassViewer : public Window {
public:
    // `place` (a storm's name, a place of the person's own) opens on that one
    explicit SatellitePassViewer(Window * parent, const std::string& place = {});

private:
    void loadPlaces();
    void fillPlaceBox();
    void recompute();
    void addPlace();
    void removePlace();
    void openMap();
    void openPassOnMap(int row);   // the master map at the pass: the place, the time and the layers that show what the instrument saw
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * placeBox{};
    QComboBox * hoursBox{};
    QComboBox * kindBox{};
    QComboBox * satelliteBox{};
    QLabel * note{};
    QTableWidget * table{};
    std::vector<SatellitePasses::Place> storms;
    std::vector<SatellitePasses::Place> mine;      // the person's own and the saved locations
    std::vector<SatellitePasses::Row> shown;   // the rows of the table
    std::string wanted;
    int generation{0};
    bool closed{false};
};

#endif  // SATELLITEPASSVIEWER_H
