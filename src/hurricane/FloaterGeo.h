// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FLOATERGEO_H
#define FLOATERGEO_H

#include <vector>

// Where a GOES floater picture (NOAA / NESDIS STAR) is on the earth. The pictures are plain latitude / longitude boxes about 18 degrees across, centred on the storm, with a
// white one-pixel graticule drawn in every 5 degrees; there is no georeferencing file. So the graticule is the reference: the lines are found in the picture itself, which also
// absorbs the little differences in where the box was centred. Pure pixel work, no Qt.
namespace FloaterGeo {
    struct Geo {
        bool fromGrid{false};      // false: no graticule was found and the box is assumed centred on the storm and 18 degrees wide
        int width{0};
        int height{0};
        double pixelsPerDegree{0.0};
        double lonAtLeft{0.0};     // degrees east of the left edge of the picture
        double latAtTop{0.0};      // of the top edge
        double x(double lon) const { return (lon - lonAtLeft) * pixelsPerDegree; }
        double y(double lat) const { return (latAtTop - lat) * pixelsPerDegree; }
        double lon(double px) const { return lonAtLeft + px / pixelsPerDegree; }
        double lat(double py) const { return latAtTop - py / pixelsPerDegree; }
    };
    // `green` is the picture's green channel (width * height bytes, rows from the top); approxLon / approxLat the storm's position (to tell which 5 degree lines are which)
    Geo locate(const unsigned char * green, int width, int height, double approxLon, double approxLat);
    // the positions (in pixels) of the vertical or horizontal lines, for the tests
    std::vector<int> graticule(const unsigned char * green, int width, int height, bool vertical);
}

#endif  // FLOATERGEO_H
