// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef AREASEARCH_H
#define AREASEARCH_H

#include <functional>
#include <QObject>
#include <QPointF>
#include <QString>
#include "radar/MapView.h"
#include "ui/Button.h"

// The "search an area" of the screens that draw many tracks (historical hurricane tracks, tornado tracks): a button turns the mode on, then a drag on the map makes a
// box and a click makes a circle round the point (the radius comes from the screen's own box). A second press of the button clears the area. The screen asks
// whether a point or a straight piece of track is inside, and paints the area.
class AreaSearch : public QObject {
public:
    // `radiusKm` is asked when a circle is made; `changed` is called when the area is set or cleared
    AreaSearch(MapView * view, Button * button, std::function<double()> radiusKm, std::function<void()> changed);
    bool active() const { return kind != None; }
    bool hitsPoint(double lat, double lon) const;
    bool hitsSegment(double lat1, double lon1, double lat2, double lon2) const;   // the straight piece between two points touches the area
    void paint(QPainter&, const MapView::Transform&, double unitsPerPixel) const;
    QString describe() const;                                                    // "in the box 24.0 to 28.0 N, -90.0 to -84.0 E"
    void clear();

private:
    bool eventFilter(QObject *, QEvent *) override;
    void toggle();
    enum Kind { None, Box, Circle } kind{None};
    double minLat{0}, maxLat{0}, minLon{0}, maxLon{0};   // a box
    double lat{0}, lon{0}, radiusKm{0};                  // a circle
    MapView * view;
    Button * button;
    std::function<double()> radius;
    std::function<void()> changed;
    bool mode{false};      // the next drag or click on the map makes an area (the map does not pan)
    bool dragging{false};
    QPointF dragStart;
    QPointF dragNow;
};

#endif  // AREASEARCH_H
