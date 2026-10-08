// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSDATA_H
#define GFSDATA_H

#include <functional>
#include <map>
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
        QString cacheFolder;       // decoded grids and idx files
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

    explicit GfsData(Config config) : config{std::move(config)} {}
    // the newest cycle whose forecast hour 0 has been published (tries the last four cycles back from now)
    bool latestRun(Run& run) const;
    // The forecast hours the run has published (the .idx of f000 .. f384 exist): a 3 hour step to 240, then 6
    static std::vector<int> forecastHours();
    // grids by key, or false with the reason
    bool load(const Run& run, int forecastHour, const std::vector<Want>& wants, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    static std::string fileUrl(const Run& run, int forecastHour);

private:
    bool one(const Run& run, int forecastHour, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const;
    Config config;
};

#endif  // GFSDATA_H
