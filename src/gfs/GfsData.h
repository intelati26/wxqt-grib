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
    struct Box {
        double west{0}, south{0}, east{0}, north{0};   // degrees, whole numbers; west < east, both within -180 .. 180
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
        std::string stat;          // an ensemble's statistic or member: "" the mean, "spr" the spread, "c00" the control and "p01" ... "p30" the members (GEFS); "m001" ... (REFS)
    };
    // One model's files. A grid that is not latitude / longitude (the blend's Lambert grid) is warped to one of `step` degrees over the box.
    struct Source {
        std::string id;                                                  // "GFS", "NBM": the model screen's name for it
        std::string label;                                               // "NOAA/NCEP GFS 0.25 degree", for under a chart
        std::function<std::string(const Run&, int forecastHour, const std::string& file)> fileUrl;   // the GRIB2 file; its index is that + ".idx". `file` is "" unless fileOf says
        std::function<std::string(const Want&)> fileOf;                  // which of a run's files holds a record (AIGFS: pressure levels in one, the surface in another); empty: one file
        float extraMissing{std::numeric_limits<float>::quiet_NaN()};     // a value the source uses for "no data" besides GRIB's own (HAFS: 9999 outside its tilted footprint); NaN: none
        bool matchRunTime{false};                                        // the index lists many times (a month of analyses, the CFS): a record is the run's own by its d= date
        std::string forecastText;                                        // a file that holds every forecast hour as records (the CFS): the index text of the hour, "{hour} hour fcst"; empty: one hour to a file
        std::string defaultDetail;                                       // what to ask of the index when a want has none ("*": any; an ensemble labels each record "ens mean" or "ens std dev")
        std::function<std::string(const std::string& fileUrl)> indexOf;   // when set: the index url made from the file url (files whose index changes its name over the years)
        std::string indexReplaces;                                       // the file's own extension, which the index's replaces (ECMWF: ".grib2" -> ".index"); empty: the suffix is added
        std::string indexSuffix{".idx"};                                 // the index is the file + this (ECMWF's is ".index")
        std::function<std::vector<GfsGrid::IdxRecord>(const std::string& text, const Want&)> parseIndex;   // NOAA's idx lines unless the source says (ECMWF's JSON lines)
        std::function<double(const Want&)> valueOffset;                  // added after the scale (Kelvin to degrees C: -273.15), 0 when none
        std::function<std::string(const Want&)> variableOf;              // a NetCDF file's name for a want (HGT at 500 mb is "H500"); the want's own variable when none
        std::function<int(const Run&, int forecastHour, const Want&)> ncBand;   // a NetCDF file that holds many times (ERA5: a month of hours, a day of hours and 37 levels): the 1-based band of the want's time and level; empty: the whole file's bands are averaged
        std::function<double(const Want&)> valueScale;                   // the factor from the file's unit to the one the charts use (ECMWF's precipitation is in meters), 1 when none
        // A server that cuts a box of one hour's fields out of the file for the asking (NOMADS's grib filter): the url for the variables and levels (NOAA's names, "HGT", "500 mb") in that box.
        // A chart of a region then fetches a few per cent of what the whole globe's messages are (the S3 buckets give nothing smaller than a message).
        std::function<std::string(const Run&, int forecastHour, const std::vector<std::string>& variables, const std::vector<std::string>& levels, const Box&)> subsetUrl;
        // A source with no index: one file per field and hour that holds one message for each member of the ensemble (Canada's GEPS). A want's `stat` then says which of them is wanted:
        // "" the mean of the members, "spr" their standard deviation, or a member's name that `memberBand` turns into the band to read.
        bool wholeFile{false};
        std::function<int(const std::string& stat)> memberBand;           // the 1-based message of a member's name ("c00" 1, "p01" 2 ...); 0 when it is not one
        // Where a `wholeFile` field may be, in the order to try (a file that is in the test area until it is in the real one, the GEFS cycle that is the run's own or the one before). A url
        // may end in a name with '?' for one character and '*' for any: the directory is listed and the first match is the file. Empty: just fileUrl.
        std::function<std::vector<std::string>(const Run&, int forecastHour, const std::string& file)> candidates;
        // A source read by code the registry does not know (SPC's Zarr store): the loader registered for its id (setLoader) makes the field's grid on a latitude / longitude grid. The cache,
        // the several fields of a chart at once and the run search are as for any other. Without a loader (a build without it) every field fails with a message.
        bool custom{false};
        bool netcdf{false};                                               // a `wholeFile` source whose files are NetCDF: want.variable is the variable's name in them, read by range (they are 0.5 to 1 GB)
        std::string probeFile;                                           // the file whose index says the run is there
        int cycleHours{6};                                               // runs are made this often
        int lagHours{3};                                                 // and a run is looked for from this long after its time
        int probeHour{0};                                                // the forecast hour whose index says the run is there
        int cyclesToTry{5};
        int maxParallel{8};                                              // fetches and decodes going at once (NOMADS is asked for fewer)
        struct Warp {
            bool enabled{false};
            double step{0.025};
            double west{-127.0}, south{22.0}, east{-65.0}, north{52.0};
        } warp;
    };
    // Every model's Source is made from its definition file (GfsModelFile.h; GfsBuiltInModels.cpp for those that ship with the program). A model whose files need code to be read has a loader:
    // a Source with `custom` is read by the function registered for its id (the SPC REFS Zarr store, spcrefs/SpcRefsLoader.cpp).
    using Loader = std::function<bool(const Run&, int forecastHour, const Want&, GfsGrid::Grid&, std::string& error)>;
    static void setLoader(const std::string& sourceId, Loader loader);

    // a field at a forecast hour (a precipitation period needs the running total at two hours)
    struct Need {
        int hour;
        Want want;
    };

    explicit GfsData(Config config, Source source) : config{std::move(config)}, source{std::move(source)} {}
    GfsData(const GfsData& other) : config{other.config}, source{other.source}, box{other.box}, boxEnabled{other.boxEnabled} {}
    // Fetch only this box when every field asked for can be had that way (otherwise the whole messages, as always): the grids come back as a regional grid, not a global one.
    void setBox(const Box& wanted) {
        box = wanted;
        boxEnabled = static_cast<bool>(source.subsetUrl);
    }
    // the newest cycle whose probe hour has been published (tries the last few cycles back from now)
    bool latestRun(Run& run) const;
    const Source& model() const { return source; }
    // The forecast hours the run has published (the .idx of f000 .. f384 exist): a 3 hour step to 240, then 6
    static std::vector<int> forecastHours();
    // grids by key, or false with the reason. A need whose record does not exist at hour 0 ("anl" has no precipitation) is simply absent from out; any other missing record fails.
    bool load(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    bool load(const Run& run, int forecastHour, const std::vector<Want>& wants, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const;
    std::string fileUrl(const Run& run, int forecastHour, const std::string& file = "") const { return source.fileUrl(run, forecastHour, file); }
    std::string indexUrl(const Run& run, int forecastHour, const std::string& file = "") const {
        auto url = source.fileUrl(run, forecastHour, file);
        if (source.indexOf) {
            return source.indexOf(url);
        }
        // (more than one ending can be given, "|" between: the first the file has is the one taken off)
        for (size_t from = 0; !source.indexReplaces.empty() && from <= source.indexReplaces.size();) {
            auto end = source.indexReplaces.find('|', from);
            end = end == std::string::npos ? source.indexReplaces.size() : end;
            const auto ending = source.indexReplaces.substr(from, end - from);
            if (!ending.empty() && url.size() >= ending.size() && url.compare(url.size() - ending.size(), ending.size(), ending) == 0) {
                url.resize(url.size() - ending.size());
                break;
            }
            from = end + 1;
        }
        return url + source.indexSuffix;
    }
    // the records downloaded for a run and hour (in this session or an earlier one still in the cache), joined into one valid GRIB2 file ("" if none): GRIB messages stand alone, so joining is all it takes
    QString partialGrib(const Run& run, int forecastHour, const std::string& file = "") const;
    // Whether a run has what `needs` asks for, without downloading any field: the index of each file lists the records (or, for a source with no index, the file answers; a NetCDF file's
    // variables and a Zarr store's arrays are not looked into). `detail` says what was found or what is missing.
    bool check(const Run& run, const std::vector<Need>& needs, std::string& detail) const;

private:
    bool one(const Run& run, int forecastHour, const std::string& file, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const;
    bool oneWhole(const Run& run, int forecastHour, const std::string& file, const Want& want, GfsGrid::Grid& out, std::string& error) const;
    std::vector<std::string> urlsFor(const Run& run, int forecastHour, const std::string& file) const;
    std::string resolveUrl(const std::string& url) const;   // a url with '?' / '*' in its name: the listed file that matches (empty when none)   // a `wholeFile` source's field
    std::string fileFor(const Want& want) const { return source.fileOf ? source.fileOf(want) : std::string{}; }
    // the fields of one hour out of a box request; false when it cannot be had (the caller then goes the usual way)
    bool loadBox(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out) const;
    Config config;
    Source source;
    Box box;
    bool boxEnabled{false};
    mutable std::mutex partialMutex;
};

#endif  // GFSDATA_H
