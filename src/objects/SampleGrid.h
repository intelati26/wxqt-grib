// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SAMPLEGRID_H
#define SAMPLEGRID_H

#include <map>
#include <string>
#include <vector>
#include <QString>

// A coarse geo-referenced grid parsed from a gdal "XYZ" dump ("lon lat value"
// per line, row-major from the top-left, lon ascending within a row), for a
// point-value hover read-out over a rendered PNG that shares the same extent.
// Shared by any viewer with a hover crosshair (GribViewer, SpcPostViewer, ...).
class SampleGrid {
public:
    int cols = 0;
    int rows = 0;
    double x0 = 0.0;   // centre lon of column 0
    double y0 = 0.0;   // centre lat of the top row
    double dx = 0.0;   // > 0, lon step
    double dy = 0.0;   // < 0, lat step (rows run north -> south)
    std::vector<float> values;   // row-major, top row first

    static SampleGrid load(const QString& path);

    bool valueAt(double lon, double lat, double& out) const;

    // Maps a 0..1 fraction of the source image to the containing cell's
    // centre lon/lat (out) and that cell's own 0..1 fraction (markerFx/Fy) -
    // so a crosshair can be snapped to the exact cell the value came from
    // instead of floating at the raw cursor position. Returns false if
    // fx/fy fall outside the grid.
    bool snap(double fx, double fy, double& lon, double& lat, double& markerFx, double& markerFy) const;
};

// Parsed grids keyed by sidecar path, so a hover over the same frame parses its file once. Emptied when it
// grows past `limit` entries (a long animation sweep), which is cheaper than tracking use order.
class SampleGridCache {
public:
    explicit SampleGridCache(size_t limit = 40) : limit{limit} {}
    // nullptr for an empty path
    const SampleGrid * get(const std::string& path);
    void clear() { grids.clear(); }

private:
    size_t limit;
    std::map<std::string, SampleGrid> grids;
};

#endif  // SAMPLEGRID_H
