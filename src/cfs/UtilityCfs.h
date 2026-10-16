// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYCFS_H
#define UTILITYCFS_H

#include <string>
#include <vector>
#include "gfs/GfsData.h"

// NOAA's Climate Forecast System version 2 (CFSv2) on the open data bucket noaa-cfs-pds: four runs a day (00, 06, 12, 18 UTC), each of four members (01 to 04), nine months long, 1 degree.
// A run's folder holds the 6 hourly fields (time_grib_NN: one file for each variable, the forecast hours as its messages, an index beside it) and the monthly means (monthly_grib_NN: one file
// for each month and family of fields). The fields are cut out of the files by their byte ranges, as every other model's are (GfsData), so a map costs about 100 to 300 KB.
namespace UtilityCfs {
    struct Variable {
        std::string key;        // "t2m"
        std::string label;      // "2 m temperature"
        std::string variable;   // NOAA's name in the index: "TMP"
        std::string level;      // "2 m above ground"
        std::string dailyFile;  // the 6 hourly file's stem: "tmp2m"
        std::string family;     // the monthly file's: "flxf" (flux: surface fields), "pgbf" (pressure levels)
        std::string units;      // as shown
        double scale{1.0};      // from the file's unit to the shown one
        double offset{0.0};
    };
    const std::vector<Variable>& variables();
    const Variable * find(const std::string& key);

    using Run = GfsData::Run;
    // the newest run published (tries the last few cycles)
    bool latestRun(Run& run);
    // The runs of the last `count` cycles ending at `newest`, newest first. Member 01 of every cycle is a nine month run (the other members are 110 days at 00Z and 45 days at 06, 12 and 18Z),
    // so the newest four cycles' member 1 are four whole forecasts started six hours apart: a lagged ensemble of the whole length.
    std::vector<Run> recentRuns(const Run& newest, int count);
    // a 6 hourly field: forecast hour 6, 12 ... 6552
    bool daily(const Run& run, int member, const Variable& variable, int forecastHour, GfsGrid::Grid& out, std::string& error);
    // a month's mean: yyyymm is the month ("202611"); the run's own month is the part still ahead of it
    bool monthly(const Run& run, int member, const Variable& variable, const std::string& yyyymm, GfsGrid::Grid& out, std::string& error);
    // a field's value at a point for several forecast hours (or months) of one member, the fields not kept: each is read, sampled and dropped. A value that could not be had is NaN; false
    // when none could (error says why). lon east positive.
    bool pointSeries(const Run& run, int member, const Variable& variable, const std::vector<int>& hours, double lat, double lon, std::vector<float>& values, std::string& error);
    bool pointMonthly(const Run& run, int member, const Variable& variable, const std::vector<std::string>& months, double lat, double lon, std::vector<float>& values, std::string& error);
    // the months a run's monthly files cover, from its own month: "202610" ... ("202707")
    std::vector<std::string> months(const Run& run);
    // the run's own time as a QDateTime-free text: "2026-10-14 00Z"
    std::string runText(const Run& run);
}

#endif  // UTILITYCFS_H
