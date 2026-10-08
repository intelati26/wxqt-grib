// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSDATA_H
#define GFSDATA_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <QByteArray>
#include <QString>
#include "gfs/GfsGrid.h"

// The GFS 0.25 degree fields from NOAA's open data bucket: the records wanted are cut out of the file by their byte ranges (the .idx says where), decoded with GDAL's
// gdal_translate, and kept on disk as compressed float grids. Nothing here knows about the screen; the network and the GDAL folder are passed in so the tests and the app share it.
class GfsData {
public:
    struct Config {
        std::function<QByteArray(const std::string& url, long long start, long long end)> bytes;   // end < 0: the whole file / listing
        std::string gdalBin;       // the folder holding gdal_translate
        QString cacheFolder;       // decoded grids and the partial GRIB; the caller removes it when the screen using it closes
    };
    struct Run {
        std::string date;          // "20261008"
        std::string cycle;         // "00", "06", "12", "18"
        std::string id() const { return date + cycle; }
    };
    struct Want {
        std::string key;           // what the chart calls it
        std::string variable;      // "HGT"
        std::string level;         // "500 mb"
        std::string forecast;      // "" for any, or "0-6 hour acc fcst"
    };

    // a field at a forecast hour (a precipitation period needs the running total at two hours)
    struct Need {
        int hour;
        Want want;
    };

    explicit GfsData(Config config) : config{std::move(config)} {}
    GfsData(const GfsData& other) : config{other.config} {}
    // the newest cycle whose forecast hour 0 has been published (tries the last four cycles back from now)
    bool latestRun(Run& run) const;
    // The forecast hours the run has published (the .idx of f000 .. f384 exist): a 3 hour step to 240, then 6
    static std::vector<int> forecastHours();
    // grids by key, or false with the reason. A need whose record does not exist at hour 0 ("anl" has no precipitation) is simply absent from out; any other missing record fails.
    bool load(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    bool load(const Run& run, int forecastHour, const std::vector<Want>& wants, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    static std::string fileUrl(const Run& run, int forecastHour);
    // the records downloaded so far for a run and hour, joined into one valid GRIB2 file in the cache folder as they arrive ("" if none yet): GRIB messages stand alone, so appending is all it takes
    QString partialGrib(const Run& run, int forecastHour) const;

private:
    bool one(const Run& run, int forecastHour, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const;
    Config config;
    mutable std::mutex partialMutex;
};

#endif  // GFSDATA_H
