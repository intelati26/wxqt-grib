// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MAPVIEW_H
#define MAPVIEW_H

#include <functional>
#include <QObject>
#include <QPointF>
#include "radar/MapWidget.h"
#include "ui/Window.h"

// What every screen that puts its own data on a MapWidget needs, in one place: the map is a square that fills its window, panned and zoomed
// by the owner, with latitude / longitude <-> pixel conversion (the radar projection is Mercator: u = (ax * lon + bx) * zoom + xPos,
// v = (ay * mercator(lat) + by) * zoom + yPos, in window units that are 1000 wide from -500 and 1000 tall from -250), the pointer position
// for zooming about it, and a hover callback. The paint callbacks stay with the owner (`map()->dataLayer` / `topLayer`).
// (The MRMS and Rivers screens each still carry their own copy of this; they can move onto it.)
class MapView : public QObject {
public:
    MapView(Window * parent, int side);
    MapWidget * map() const { return widget; }

    // lat / lon -> window units (what `topLayer` paints in) for many points: the projection is read once, the pan / zoom as it is now
    struct Transform {
        double ax, bx, ay, by, zoom, xPos, yPos;
        QPointF operator()(double lat, double lon) const {
            return QPointF{(ax * lon + bx) * zoom + xPos, (ay * mercator(lat) + by) * zoom + yPos};
        }
    };
    Transform transform() const;
    QPointF toUnits(double lat, double lon) const;
    QPointF toPixels(double lat, double lon) const;    // widget pixels (what a click or the pointer gives)
    double unitsPerPixel() const;
    bool inView(const QPointF& units, double margin = 20.0) const;
    void showRegion(double minLat, double maxLat, double minLon, double maxLon);   // the box fills the square, centred
    void fit(int availableWidth, int availableHeight);                              // a square no larger than the space
    void zoomBy(double factor);
    void panBy(double dxPixels, double dyPixels);

    std::function<void(const QPointF&)> onPointer;   // the pointer moved over the map (pixels)
    std::function<void()> onLeave;
    bool pointerInside{false};
    QPointF pointer;

    static double mercator(double lat);

private:
    struct Coefficients {
        double ax;
        double bx;
        double ay;
        double by;
    };
    Coefficients coefficients() const;
    bool eventFilter(QObject *, QEvent *) override;

    MapWidget * widget{};
};

#endif  // MAPVIEW_H
