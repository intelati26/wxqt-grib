// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYMRMS_H
#define UTILITYMRMS_H

#include <string>
#include <vector>
#include <QDateTime>
#include <QByteArray>
#include <QVector>
#include <QRgb>

using std::string;
using std::vector;

// NOAA's MRMS (Multi-Radar Multi-Sensor) CONUS products from https://mrms.ncep.noaa.gov/2D/: one small gzipped GRIB2
// per product and scan, about every two minutes, kept for about a day. GDAL decodes a scan to its native grid (no
// resampling); the MRMS viewer then looks each screen pixel up in that grid through the radar widget's own
// projection, so the detail stays at full resolution at any zoom. Every call blocks on the network and GDAL: use
// them off the UI thread. A failure comes back as a message, never silently.
namespace UtilityMrms {
    // the grid MRMS CONUS products cover (degrees)
    constexpr double west = -130.0;
    constexpr double east = -60.0;
    constexpr double south = 20.0;
    constexpr double north = 55.0;

    struct Stop {
        double value;
        int r;
        int g;
        int b;
    };

    struct Product {
        string id;                 // the server's folder name, e.g. "MergedReflectivityQCComposite"
        string label;
        string units;
        double validMin;           // values below this are "nothing here" (transparent)
        double hi;                 // the top of the colour scale
        vector<Stop> stops;        // colour table, ascending by value
    };

    struct Scan {
        string file;               // MRMS_<product>_<level>_<yyyymmdd-hhmmss>.grib2.gz
        QDateTime utc;
    };

    // the products' common native grid: 7000 x 3500 cells of 0.01 degrees, north-up, from (west, north)
    constexpr int columns = 7000;
    constexpr int rows = 3500;
    constexpr double cell = 0.01;

    // one decoded scan at the grid's full resolution: an 8-bit index per cell, 0 = nothing / transparent, i >= 1 is
    // the value validMin + (i - 1) * step. Kept zlib-compressed (most of a scan is empty), so a loop is cheap.
    struct Frame {
        QByteArray packed;
        double validMin{0.0};
        double step{1.0};
        QDateTime utc;
        QByteArray indices() const { return qUncompress(packed); }
        double valueAt(int index) const { return validMin + (index - 1) * step; }
    };

    const vector<Product>& products();
    // the scans the server has for a product, oldest first
    bool scans(const Product&, vector<Scan>& out, string& error);
    bool frame(const Product&, const Scan&, Frame& out, string& error);
    // the colour table of a product at 256 entries (index 0 transparent)
    QVector<QRgb> colorTable(const Product&);
}

#endif  // UTILITYMRMS_H
