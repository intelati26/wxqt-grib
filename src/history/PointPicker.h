// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef POINTPICKER_H
#define POINTPICKER_H

#include <functional>
#include <memory>
#include <QLabel>
#include "radar/MapView.h"
#include "ui/Window.h"

// A map to click any point on (zoom and pan as on the other maps); "Use this point" gives its latitude and longitude back.
class PointPicker : public Window {
public:
    PointPicker(Window * parent, double lat, double lon, bool have, const std::function<void(double, double)>& chosen);

private:
    void paintMarker(QPainter&);
    void closeEventCustom() override { closed = true; }
    std::unique_ptr<MapView> view;
    QLabel * label{};
    double lat{0.0}, lon{0.0};
    bool have{false};
    bool closed{false};
};

#endif  // POINTPICKER_H
