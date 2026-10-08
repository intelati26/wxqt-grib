// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSGRID_H
#define GFSGRID_H

#include <string>
#include <vector>

// The numerical half of the GFS charts, with no Qt: the GRIB index lines, a regular latitude / longitude grid with sampling and derived fields, and contour lines.
namespace GfsGrid {
    struct IdxRecord {
        int number{0};
        long long start{0};
        long long end{-1};          // the byte before the next record (-1 for the last one: to the end of the file)
        std::string variable;       // "HGT"
        std::string level;          // "500 mb"
        std::string forecast;       // "anl", "6 hour fcst", "0-6 hour acc fcst"
    };
    // "12:5483620:d=2026100800:HGT:500 mb:6 hour fcst:" per line
    std::vector<IdxRecord> parseIdx(const std::string& text);
    // the first record of that variable at that level ("TMP", "2 m above ground"); forecast, if given, must match too (a trailing * matches a start: "0-*")
    const IdxRecord * find(const std::vector<IdxRecord>& records, const std::string& variable, const std::string& level, const std::string& forecast = "");

    // Values at cell centers on a regular grid, row 0 = the northernmost, column 0 = lon0. Longitudes wrap when the grid spans the globe.
    struct Grid {
        int columns{0};
        int rows{0};
        double lon0{0.0};     // degrees east of the first column
        double lat0{90.0};    // latitude of the first row
        double step{0.25};    // degrees, both ways
        std::vector<float> values;

        bool empty() const { return values.empty(); }
        bool global() const { return columns * step > 359.0; }
        float at(int column, int row) const { return values[static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column)]; }
        // bilinear; NaN outside the grid (north or south of its rows, or off the edge of a regional grid)
        float sample(double lon, double lat) const;
    };

    // speed = sqrt(u^2 + v^2); the same grid for both inputs is required
    Grid speed(const Grid& u, const Grid& v);
    // relative vorticity dv/dx - du/dy on the sphere, in 1/s (the poles' rows are left NaN)
    Grid vorticity(const Grid& u, const Grid& v);
    // horizontal divergence du/dx + dv/dy on the sphere, in 1/s (the poles' rows are left NaN)
    Grid divergence(const Grid& u, const Grid& v);
    // a - b
    Grid difference(const Grid& a, const Grid& b);
    // out = in * scale + offset
    Grid scaled(const Grid& in, double scale, double offset = 0.0);
    // the same grid, smoothed by a 3 x 3 average repeated passes times
    Grid smoothed(const Grid& in, int passes);

    struct Point {
        double lon;
        double lat;
    };
    using Line = std::vector<Point>;
    // Lines of equal value inside west..east / south..north (west < east; longitudes may be given beyond 180). Segments are joined into lines; a closed line repeats its first point.
    std::vector<Line> contour(const Grid& grid, double level, double west, double south, double east, double north);

    struct Extreme {
        double lon;
        double lat;
        float value;
        bool high;
    };
    // The highs and lows of a field (pressure): a cell that is the extreme of everything within radius degrees, inside the box
    std::vector<Extreme> extremes(const Grid& grid, double radius, double west, double south, double east, double north);
}

#endif  // GFSGRID_H
