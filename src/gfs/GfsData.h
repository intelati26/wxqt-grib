// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSDATA_H
#define GFSDATA_H

#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <QByteArray>
#include <QString>
#include "gfs/GfsGrid.h"

// Model fields from NOAA's open data buckets (the GFS 0.25 degree, the National Blend of Models): the records wanted are cut out of the file by their byte ranges (the .idx says
// where), decoded with GDAL (gdal_translate for a grid that is already latitude / longitude, gdalwarp for one that is not), and kept on disk as compressed float grids. What differs
// between the models is only their Source: where the files are, how often a run is made, and whether the grid has to be warped. Nothing here knows about the screen; the network
// and the GDAL folder are passed in so the tests and the app share it.
class GfsData {
public:
    struct Config {
        std::function<QByteArray(const std::string& url, long long start, long long end)> bytes;   // end < 0: the whole file / listing
        std::string gdalBin;       // the folder holding gdal_translate
        QString cacheFolder;       // decoded grids and the GRIB messages they came from (GfsCache keeps the folder: shared by the screens, kept between runs)
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
        std::string detail;        // "" for the plain field; the blend's "prob >0.254", "50% level" ask for those records instead
        std::string stat;          // an ensemble's statistic: "" the mean, "spr" the spread (GEFS)
    };
    // One model's files. A grid that is not latitude / longitude (the blend's Lambert grid) is warped to one of `step` degrees over the box.
    struct Source {
        std::string id;                                                  // "GFS", "NBM": the model screen's name for it
        std::string label;                                               // "NOAA/NCEP GFS 0.25 degree", for under a chart
        std::function<std::string(const Run&, int forecastHour, const std::string& file)> fileUrl;   // the GRIB2 file; its index is that + ".idx". `file` is "" unless fileOf says
        std::function<std::string(const Want&)> fileOf;                  // which of a run's files holds a record (AIGFS: pressure levels in one, the surface in another); empty: one file
        float extraMissing{std::numeric_limits<float>::quiet_NaN()};     // a value the source uses for "no data" besides GRIB's own (HAFS: 9999 outside its tilted footprint); NaN: none
        std::string defaultDetail;                                       // what to ask of the index when a want has none ("*": any; an ensemble labels each record "ens mean" or "ens std dev")
        std::string probeFile;                                           // the file whose index says the run is there
        int cycleHours{6};                                               // runs are made this often
        int lagHours{3};                                                 // and a run is looked for from this long after its time
        int probeHour{0};                                                // the forecast hour whose index says the run is there
        int cyclesToTry{5};
        struct Warp {
            bool enabled{false};
            double step{0.025};
            double west{-127.0}, south{22.0}, east{-65.0}, north{52.0};
        } warp;
    };
    static Source gfs();
    static Source nbm();
    static Source aigfs();
    static Source gefs();
    // The Rapid Refresh Forecast System's 3 km CONUS grid (Lambert, warped to latitude / longitude): hourly runs, the 2D fields and the pressure levels as files of their own
    static Source rrfs();
    // The Rapid Refresh Forecast System's ensemble (REFS): 5 members at 3 km (the `stat` of a want names the member, "m001" ... "m005") and the ready-made products of its own (the stat "ens:avrg",
    // "ens:eas" for the probabilities, "ens:ffri" for flash flood risk)
    static Source refs();
    // The wave models: GFS-Wave's global 0.16 degree grid, and the GEFS-Wave control member on 0.25 (the ensemble mean file has no wind components and lists its swells ambiguously) (latitude / longitude grids, one file for the sea state at each hour)
    static Source gfsWave();
    static Source gefsWave();
    // The hurricane model for one active storm ('model' "HAFSA" or "HAFSB", 'storm' "09l": the NHC number and basin letter). Its grid follows the storm and is only there while the storm is.
    static Source hafs(const std::string& model, const std::string& storm);

    // a field at a forecast hour (a precipitation period needs the running total at two hours)
    struct Need {
        int hour;
        Want want;
    };

    explicit GfsData(Config config, Source source = gfs()) : config{std::move(config)}, source{std::move(source)} {}
    GfsData(const GfsData& other) : config{other.config}, source{other.source} {}
    // the newest cycle whose probe hour has been published (tries the last few cycles back from now)
    bool latestRun(Run& run) const;
    const Source& model() const { return source; }
    // The forecast hours the run has published (the .idx of f000 .. f384 exist): a 3 hour step to 240, then 6
    static std::vector<int> forecastHours();
    // grids by key, or false with the reason. A need whose record does not exist at hour 0 ("anl" has no precipitation) is simply absent from out; any other missing record fails.
    bool load(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    bool load(const Run& run, int forecastHour, const std::vector<Want>& wants, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    std::string fileUrl(const Run& run, int forecastHour, const std::string& file = "") const { return source.fileUrl(run, forecastHour, file); }
    // the records downloaded for a run and hour (in this session or an earlier one still in the cache), joined into one valid GRIB2 file ("" if none): GRIB messages stand alone, so joining is all it takes
    QString partialGrib(const Run& run, int forecastHour, const std::string& file = "") const;

private:
    bool one(const Run& run, int forecastHour, const std::string& file, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const;
    std::string fileFor(const Want& want) const { return source.fileOf ? source.fileOf(want) : std::string{}; }
    Config config;
    Source source;
    mutable std::mutex partialMutex;
};

#endif  // GFSDATA_H
