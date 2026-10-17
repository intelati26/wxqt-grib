// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYCFS_H
#define UTILITYCFS_H

#include <atomic>
#include <string>
#include <QDateTime>
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
    std::vector<Run> recentRuns(const Run& newest, int count, int stepHours = 6);
    // a 6 hourly field: forecast hour 6, 12 ... 6552
    bool daily(const Run& run, int member, const Variable& variable, int forecastHour, GfsGrid::Grid& out, std::string& error);
    // a month's mean: yyyymm is the month ("202611"); the run's own month is the part still ahead of it
    bool monthly(const Run& run, int member, const Variable& variable, const std::string& yyyymm, GfsGrid::Grid& out, std::string& error);
    // a field's value at a point for several forecast hours (or months) of one member, the fields not kept: each is read, sampled and dropped. A value that could not be had is NaN; false
    // when none could (error says why). lon east positive.
    bool pointSeries(const Run& run, int member, const Variable& variable, const std::vector<int>& hours, double lat, double lon, std::vector<float>& values, std::string& error, std::atomic<int> * progress = nullptr);
    // the same as departures from the CFS normal (or in standard deviations); the month in progress stays NaN
    bool pointMonthlyDeparture(const Run& run, int member, const Variable& variable, const std::vector<std::string>& months, bool inDeviations, int baseline, double lat, double lon, std::vector<float>& values, std::string& error, std::atomic<int> * progress = nullptr);
    bool pointMonthly(const Run& run, int member, const Variable& variable, const std::vector<std::string>& months, double lat, double lon, std::vector<float>& values, std::string& error, std::atomic<int> * progress = nullptr);
    // The CFS's own normal: NOAA's calibration climatologies of the reforecasts (1999-2010 or 1982-2010; mean "fclm" and standard deviation "fstd" of the monthly means, by start date, cycle and lead
    // month: lead 1 is the month after the run's own). Each is a field of NCEI's reforecast bucket read by byte range and kept for good (PermanentCache "climate") packed in 16 bits, a few tens
    // of KB. `leadMonth` 1-9; the start date is the run's (the nearest day with a file if its own has none). The values are in the shown unit.
    bool climatology(const Variable& variable, const Run& run, int leadMonth, bool deviation, int baseline, GfsGrid::Grid& out, std::string& error);
    // The normal of one start date as the file has it (cycle 00, kept for good): what climatology() blends. Only the 1st and the 16th of each month are asked for by it, about 15 days apart.
    bool climatologyOn(const Variable& variable, const std::string& yyyymmdd, int leadMonth, bool deviation, int baseline, GfsGrid::Grid& out, std::string& error);
    // a member's monthly mean minus the normal of the run's start date (or divided by the standard deviation when `inDeviations`); the run's own month, which has no normal, fails
    bool monthlyDeparture(const Run& run, int member, const Variable& variable, const std::string& yyyymm, bool inDeviations, int baseline, GfsGrid::Grid& out, std::string& error);
    // The GFS beside it, for the same plume window (the GFS is a run every six hours to 16 days): the newest run, and a field's values at a point for forecast hours, each read as a small box
    // of the field (the box request of NOMADS when it answers, the whole message from the open bucket when it does not).
    const std::vector<Variable>& gfsVariables();
    bool gfsLatestRun(Run& run);
    bool gfsPointSeries(const Run& run, const Variable& variable, const std::vector<int>& hours, double lat, double lon, std::vector<float>& values, std::string& error, std::atomic<int> * progress = nullptr);
    // The GEFS (31 members, 0.5 degree, 16 days) for the plume window: the newest run, and one member's (c00, p01 ... p30; "" the mean of them, "spr" their spread) value at a point at forecast hours
    const std::vector<Variable>& gefsVariables();
    bool gefsLatestRun(Run& run);
    bool gefsPointSeries(const Run& run, const Variable& variable, const std::string& member, const std::vector<int>& hours, double lat, double lon, std::vector<float>& values, std::string& error, std::atomic<int> * progress = nullptr);
    // the months a run's monthly files cover, from its own month: "202610" ... ("202707")
    std::vector<std::string> months(const Run& run);
    // the run's own time (UTC)
    QDateTime runTime(const Run& run);
    // the run's own time as a QDateTime-free text: "2026-10-14 00Z"
    std::string runText(const Run& run);
}

#endif  // UTILITYCFS_H
