// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYDROUGHT_H
#define UTILITYDROUGHT_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <QDate>

// The U.S. Drought Monitor as shapes (its KMZ: one multipolygon for each category, D0 to D4, where a worse category lies inside the milder ones), the areas of the United States to look
// at it by (the Census Bureau's cartographic boundary files of the states and the counties), and what comes of putting the two together: the share of an area in each category on a
// date, and how that changed between two dates. Everything here works on the shapes, so the map is sharp at any zoom and the numbers follow the area the user picked.
namespace UtilityDrought {
    using Ring = std::vector<std::pair<double, double>>;     // (longitude, latitude)
    using Polygon = std::vector<Ring>;                        // the outer ring, then its holes
    struct Monitor {
        QDate valid;                                          // the Tuesday the map is valid for
        std::vector<Polygon> shapes[5];                       // by category D0 .. D4
    };
    // the KMZ file's bytes; false with the reason when it is not a drought map
    bool parseKmz(const std::string& kmz, Monitor& out, std::string& error);

    struct Area {
        std::string id;                                       // "08" (a state's FIPS code), "08031" (a county's)
        std::string name;                                     // "Colorado", "Denver County, CO"
        std::string group;                                    // the state a county is in, "States" for a state, "United States" for the whole
        std::vector<Polygon> shapes;
        double west{0}, south{0}, east{0}, north{0};
    };
    // a zip of a Census cartographic boundary shapefile (states or counties); the territories outside the contiguous states are left out unless `all`
    bool parseAreas(const std::string& zip, bool counties, bool all, std::vector<Area>& out, std::string& error);

    // The category of every cell of a grid of squares of `step` degrees over a box: 0 none, 1 to 5 D0 to D4 (the worst that lies over the cell's center)
    struct Raster {
        int columns{0}, rows{0};
        double west{0}, north{0}, step{0.05};
        std::vector<uint8_t> category;
    };
    Raster rasterize(const Monitor& monitor, double west, double south, double east, double north, double step = 0.05);
    // 1 where the cell's center is inside the area
    std::vector<uint8_t> mask(const Raster& like, const std::vector<Area>& areas);

    // the share of the masked cells (weighted by the cosine of latitude, so by area) in D0 or worse ... D4, and in none: as the Monitor's own statistics give them
    struct Share {
        double none{0}, atLeast[5]{};                         // percent
        double dsci() const { return atLeast[0] + atLeast[1] + atLeast[2] + atLeast[3] + atLeast[4]; }   // the Drought Severity and Coverage Index, 0 to 500
    };
    Share share(const Raster& raster, const std::vector<uint8_t>& mask);
    // how many categories each cell moved between two dates (the later less the earlier: positive is worse), 0 outside the mask or where both are none; cells of another size are not compared
    std::vector<int8_t> change(const Raster& earlier, const Raster& later);
}

#endif  // UTILITYDROUGHT_H
