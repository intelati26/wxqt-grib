// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef BARBGRID_H
#define BARBGRID_H

#include <algorithm>
#include <cmath>

// Where the wind barbs of every map and chart go, one rule for all of them: points of whole degrees of latitude and longitude (the world grid), the step chosen from the zoom so that the barbs stay
// about the same distance apart on the screen or the picture, a size smaller and a step denser on the whole earth. Toward the poles, where the meridians meet, a row keeps every second, third ...
// point of the grid. Pixels are those of the picture the barbs are drawn on.
namespace BarbGrid {
    struct Look {
        double step{1.0};      // degrees between the points
        double spacing{48.0};  // the distance the barbs are meant to keep (pixels)
        double length{24.0};   // the staff (pixels)
    };

    // spanDegrees: the longitudes the picture shows; pixelsPerDegree: along the equator; marks: 1 on the screen, larger for a picture of the world made at a larger size
    inline Look look(double spanDegrees, double pixelsPerDegree, double marks = 1.0) {
        Look l;
        l.spacing = (spanDegrees > 150.0 ? 44.0 : spanDegrees > 80.0 ? 52.0 : 48.0) * marks;
        l.length = (spanDegrees > 150.0 ? 20.0 : 24.0) * marks;
        l.step = 30.0;
        for (const double candidate : {0.1, 0.125, 0.2, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0, 30.0}) {
            if (candidate * pixelsPerDegree >= l.spacing * 0.92) {
                l.step = candidate;
                break;
            }
        }
        return l;
    }

    // the longitude step of the row at a latitude: the grid's step, or a multiple of it where the meridians have come together
    inline double lonStep(double step, double lat) {
        const int every = std::clamp(static_cast<int>(std::floor(1.0 / std::max(0.05, std::cos(lat * 3.14159265358979 / 180.0)) + 0.5)), 1, 8);
        return step * every;
    }
}

#endif  // BARBGRID_H
