// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSCLIMATE_H
#define GFSCLIMATE_H

#include <string>
#include "gfs/GfsGrid.h"

// The daily long-term means (1991-2020) of the NCEP/NCAR reanalysis from NOAA PSL, for the anomaly charts. The files are large but every day and level is a chunk of its own,
// so GDAL (through /vsicurl/) downloads just the slice asked for. Slices are kept for good (PermanentCache, "climate") as 16 bit numbers, one every fourth day:
// the mean is a smooth seasonal curve, so the two nearest are blended in time. See docs/gfs-anomalies-plan.md.
class GfsClimate {
public:
    struct Field {
        std::string file;        // under .../ncep.reanalysis.derived/, "pressure/hgt.day.ltm.1991-2020.nc"
        std::string variable;    // "hgt"
        int level{-1};           // an index into levels(), or -1 for a surface field
    };
    static Field height(int hPa);   // geopotential height (m) at a pressure level, or an invalid field (empty file) if the reanalysis has no such level
    static Field seaLevelPressure();   // Pa
    static Field temperature(int hPa); // K, at a pressure level
    static Field precipitableWater();   // kg/m2 (a surface field)

    // gdalBin: the folder with gdal_translate. baseUrl: where the files are (overridable for tests)
    explicit GfsClimate(std::string gdalBin, std::string baseUrl = "https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/") : gdalBin{std::move(gdalBin)}, baseUrl{std::move(baseUrl)} {}

    // The climatological field on the date, a 2.5 degree grid (lon 0..357.5, lat 90..-90); false with the reason (nothing stored and no connection, say)
    bool at(const Field& field, int dayIndex, GfsGrid::Grid& out, std::string& error) const;

    // The standard deviation of a field's daily value about its daily mean (the 30 years 1991-2020 of the reanalysis' daily averages, the days within four of the one asked for pooled), on the
    // same grid. It is worked out once, in the background (about a thousand slices from the web for each field, kept for good): until then this is false with a message that says how far it is.
    bool deviation(const Field& field, int dayIndex, GfsGrid::Grid& out, std::string& error) const;

    // The pure parts:
    static int dayIndex(int year, int month, int day);        // 0..364; 29 February counts as 28 February (the means have 365 days)
    static const int * levels();                              // the 17 pressure levels, 1000 .. 10 hPa
    static long band(const Field& field, int dayIndex);       // the 1-based band of that day in the file (levels vary fastest)
    static void anchors(int dayIndex, int& first, int& second, double& weightOfSecond);   // the stored days around it (every fourth; after 364 comes 0 of the next year)
    static std::string pack(const GfsGrid::Grid& grid);       // 16 bit steps between neighbors, to be compressed by the cache
    static bool unpack(const std::string& bytes, GfsGrid::Grid& out);

private:
    bool anchor(const Field& field, int day, GfsGrid::Grid& out, std::string& error) const;
    bool storedDeviation(const Field& field, int day, GfsGrid::Grid& out) const;
    void buildDeviation(Field field) const;
    std::string gdalBin;
    std::string baseUrl;
};

#endif  // GFSCLIMATE_H
