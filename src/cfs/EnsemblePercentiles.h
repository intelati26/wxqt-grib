// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ENSEMBLEPERCENTILES_H
#define ENSEMBLEPERCENTILES_H

#include <map>
#include <string>
#include <vector>
#include <QByteArray>

// The members of an ensemble at one place, and what can be said about them together: percentiles of 2 m temperature and of precipitation, in time (docs/plans.md, "Ensemble percentiles at a place").
// The members come from Open-Meteo's ensemble service (GEFS, ECMWF IFS and AIFS, GEPS, ICON, UKMO): one request gives every member's hourly values for the place, so nothing is downloaded by the field.
// The statistics here need no window and no network: the parse and the numbers are tested on their own (tests/cfs/run_ensemblepct.sh).
namespace EnsemblePercentiles {
    // one variable of one model: `members[m][t]` at `times[t]` (unix seconds), NaN where a member has no value
    struct Series {
        std::vector<double> times;
        std::vector<std::vector<float>> members;
        std::string units;   // as the service says ("°C", "mm")
    };
    // The percentiles of the members at each time. `levels` are 0 to 100 (0 the lowest member, 100 the highest); `value[level][t]`; NaN where fewer than two members have a value.
    struct Fan {
        std::vector<double> times;
        std::vector<int> levels;
        std::map<int, std::vector<float>> value;
        std::vector<float> mean;
        std::vector<int> count;   // members with a value at each time
    };
    // The q (0 to 100) percentile of values, by linear interpolation between the sorted values (the usual (n-1) rule: the median of 1, 2, 3, 4 is 2.5); NaNs are left out; NaN when fewer than one value.
    float percentile(std::vector<float> values, double q);
    Fan fan(const Series& series, const std::vector<int>& levels = {0, 10, 25, 50, 75, 90, 100});
    // Each member's running total from the first time (a member's total is NaN from its first missing value on: no value is guessed).
    Series accumulated(const Series& hourly);
    // The share of members (0 to 1) whose total over each block of `hours` hours (blocks begin at a UTC hour that is a multiple of `hours`) is at least `threshold`, one value for each block: `times` of the result
    // are the blocks' ends. A block with a missing value or with fewer hours than `hours` is left out.
    struct Chance {
        std::vector<double> times;   // the end of each block
        std::vector<float> share;
        std::vector<float> median, high;   // the median and the 90th percentile of the members' block totals
    };
    Chance chanceOfAtLeast(const Series& hourly, int hours, double threshold);
    // Two models at the same hours (the times both have), each as its median and its 25 to 75 percent band, and the difference A minus B of the medians and of the means: "Euro is cooler than GEFS" as a number
    // for every hour. `daily` is the difference averaged over each UTC day that has all 24 hours (the day's start, unix seconds).
    struct Compare {
        std::vector<double> times;
        std::vector<float> aMedian, aLow, aHigh, bMedian, bLow, bHigh, difference, meanDifference;
        std::vector<float> aP10, aP90, bP10, bP90;   // the 10th and 90th percentiles (the whiskers of a box)
        std::vector<double> dayStart;
        std::vector<float> daily;
    };
    Compare compare(const Fan& a, const Fan& b);
    // The members of a series at the given times (every member, the hours the series does not have as NaN): what a spaghetti plot draws over the common hours of `compare`.
    std::vector<std::vector<float>> onTimes(const Series& series, const std::vector<double>& times);
    // The mean of the difference over the first `days` days from the first time (NaN when fewer than half of the hours have a value).
    float meanDifference(const Compare& c, double days);
    // The variables the comparison windows can show: one row each (id as the service names it, what it is, how its numbers become the person's units). A new variable is a row here.
    enum class Quantity { Temperature, Precipitation, Wind, Snow, Pressure, Percent };
    struct Variable {
        std::string id, label, title;   // "dew_point_2m", "2 m dewpoint", "Dewpoint (2 m)"
        Quantity quantity{Quantity::Temperature};
        bool accumulate{false};         // shown added up since the first hour (rain, snow)
        bool zeroBased{false};          // the axis starts at zero
        int digits{1};
    };
    const std::vector<Variable>& variables();
    const Variable * findVariable(const std::string& id);
    // A value of the service in the person's units (Settings: deg F, in, mph ... or deg C, mm, m/s ...); `difference` for a difference of two values (a temperature difference has no offset).
    double display(const Variable& variable, double value, bool difference = false);
    std::string unitLabel(const Variable& variable);
    // One variable of one model for a place (a request each, kept 20 minutes), added up from the first hour when the variable accumulates. Blocking.
    bool fetchOne(double lat, double lon, const std::string& model, const Variable& variable, Series& out, std::string& why);
    // The service: `variable` ("temperature_2m", "precipitation"), `model` ("gfs025", "ecmwf_ifs025", ...). The members' columns are `<variable>` (the control) and `<variable>_memberNN`.
    bool parse(const QByteArray& json, const std::string& variable, Series& out, std::string& why);
    std::string url(double lat, double lon, const std::string& model, const std::vector<std::string>& variables, int days = 16);
    // The models offered: id, what it is.
    struct Model { std::string id, label; };
    const std::vector<Model>& models();
    // Both variables of one model for a place: one request, blocking (call it from a thread). Kept in memory for 20 minutes.
    bool fetch(double lat, double lon, const std::string& model, Series& temperature, Series& precipitation, std::string& why);
}

#endif  // ENSEMBLEPERCENTILES_H
