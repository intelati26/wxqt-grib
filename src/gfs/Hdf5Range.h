// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HDF5RANGE_H
#define HDF5RANGE_H

#include <functional>
#include <string>
#include <QByteArray>
#include <QString>

// A variable of a NetCDF-4 file (HDF5 inside) read by byte ranges: for the systems where GDAL's NetCDF driver cannot read a remote file in parts (it needs Linux's userfaultfd) and for the files that are
// far too big to be downloaded whole (SPC's mesoanalysis files are 520 MB an hour, its RRFS fields 3.8 GB). Only the file's metadata and the chunks of the variable are read (a few megabytes), and
// written as a small classic NetCDF file (the variable, its dimensions' coordinate variables and its grid mapping) that GDAL opens from the disk as it would any file.
// What is read: the format libhdf5 writes by default (superblock 0, groups as symbol tables, B-tree chunk indexes), the variables of the root group, datasets that are contiguous, compact or chunked with
// the deflate and shuffle filters, of the integer and floating point types of 1, 2, 4 and 8 bytes (an 8 byte integer is not), and text.
namespace Hdf5Range {
    // the bytes from `start` to `end`, both included; empty on a failure
    using Fetch = std::function<QByteArray(long long start, long long end)>;
    // Writes the classic NetCDF file `outPath` for `variable`; false with the reason (the file is then to be read another way)
    // For a grid that GDAL cannot place from its projection (SPC's RRFS fields are on a rotated pole), the 2D latitude and longitude it places it by are included; they are the same for every variable of
    // the grid, so they are kept in `cacheFolder` (if given) and read from the file once.
    bool extract(const Fetch& fetch, const std::string& variable, const QString& outPath, std::string& error, const QString& cacheFolder = QString{});
}

#endif  // HDF5RANGE_H
