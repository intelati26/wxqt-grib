// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GDALRASTER_H
#define GDALRASTER_H

#include <string>
#include <vector>
#include <QString>
#include <QStringList>
#include "gfs/GfsGrid.h"

// The one way a field is got out of a file by GDAL's command line tools: gdal_translate (or gdalwarp, to a latitude / longitude grid) writes an ENVI pair (the raw floats and a header), which
// is read back as a grid. What the file is (a GRIB2 message, a NetCDF variable by range, one band of either), the scale and offset, and the cache stay with the caller.
namespace GdalRaster {
    struct Warp {
        bool enabled{false};               // a grid that is not latitude / longitude is warped to one (bilinear: smooth, with no overshoot at the edge of a rain area)
        double west{0}, south{0}, east{0}, north{0};
        double step{0};
    };
    struct Options {
        QString gdalBin;                   // the folder of the tools
        Warp warp;
        int band{0};                       // the 1-based band; 0: all of them
        std::vector<int> bandList;         // or these bands, in this order (gdal_translate only, no warp)
        QStringList extra;                 // more arguments for the tool, before the input (a NetCDF read by range: "--config", "CPL_VSIL_CURL_CHUNK_SIZE", "2097152")
        int timeoutMs{180000};
        bool requirePlain{true};           // false for a grid with cells that are not square (a Gaussian one): the step is then the longitude one
    };
    // the scratch files of a run: `base` + ".raw" and ".hdr"; a file that is already there is overwritten
    // Runs the tool on `input` and writes `base`.raw. false with the tool's own message (its error output, trimmed) when it fails.
    bool translate(const QString& input, const QString& base, const Options& options, std::string& message);
    // The ENVI pair gdal wrote (the raw floats and the header) as the geometry of a plain latitude / longitude grid and the values of every band, one band after the other. false with the reason
    // when the grid is not that or the file is not the size it says.
    bool readEnvi(const QString& rawPath, const QString& hdrPath, GfsGrid::Grid& g, int& bands, std::vector<float>& values, std::string& error, bool requirePlain = true);
    // translate, readEnvi and remove the scratch files, for the callers that have nothing to do with the message of a failure
    bool read(const QString& input, const QString& base, const Options& options, GfsGrid::Grid& g, int& bands, std::vector<float>& values, std::string& error);
    void removeScratch(const QString& base);
    // The band of a GRIB2 file that holds a field, from the file's own band list (gdalinfo, kept for each file): by the NOAA names (TMP, "2 m above ground"; "5000-2000 m above ground"; "entire
    // atmosphere"). A field there more than once (an hour's maximum and a running total) is the first. 0 with the reason when the file has none.
    int bandOf(const QString& gdalBin, const QString& grib, const std::string& variable, const std::string& level, std::string& error);
}

#endif  // GDALRASTER_H
