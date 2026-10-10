// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSHAPEFILE_H
#define UTILITYSHAPEFILE_H

#include <map>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// ESRI shapefiles (the .shp geometry with its .dbf attribute table): points, polylines and polygons, longitude and latitude in degrees. Only what NHC's
// GIS products use; the specification is ESRI's "ESRI Shapefile Technical Description" (1998) and the dBASE III table format.
class UtilityShapefile {
public:
    struct Feature {
        int shapeType{0};                                   // 1 point, 3 polyline, 5 polygon (the others are read as nothing)
        vector<vector<std::pair<double, double>>> parts;    // (lon, lat) points of each part (a ring of a polygon, a piece of a polyline, the one point)
        std::map<string, string> attributes;                // from the dbf, trimmed
    };
    static bool parse(const string& shp, const string& dbf, vector<Feature>& features);
};

#endif  // UTILITYSHAPEFILE_H
