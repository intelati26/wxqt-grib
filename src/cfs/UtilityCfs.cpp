// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/UtilityCfs.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <QDate>
#include <QDateTime>
#include <QTimeZone>
#include "gfs/GfsCache.h"
#include "gfs/GfsClimate.h"
#include "util/PermanentCache.h"
#include "gfs/GfsModels.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"

namespace {
    const std::string bucket = "https://noaa-cfs-pds.s3.amazonaws.com/";

    std::string member2(int member) {
        return std::string{member < 10 ? "0" : ""} + std::to_string(member);
    }

    GfsData::Config config() {
        GfsData::Config c;
        c.gdalBin = UtilityGrib::gdalBinDir();
        c.cacheFolder = GfsCache::folder();
        c.bytes = [] (const std::string& url, long long start, long long end) {
            return end < 0 && start == 0 ? URL::getBytesManaged(url) : URL::getBytesRangeManaged(url, start, end < 0 ? start + 4 * 1024 * 1024 * 1024LL : end);
        };
        return c;
    }

    // `monthly`: the file is one of the monthly means, and `file` is "flxf.202611"; otherwise the 6 hourly file of a variable ("tmp2m")
    GfsData::Source source(int member, bool monthly) {
        GfsData::Source s;
        s.id = std::string{monthly ? "CFSM" : "CFSD"} + member2(member);
        s.label = "NOAA/NCEP CFSv2";
        s.probeFile = "tmp2m";
        // the grid is a Gaussian one (384 x 190, 0.9375 by 0.947 degrees): put on a regular 1 degree grid
        s.warp = {true, 1.0, -180.0, -90.0, 180.0, 90.0};
        s.cycleHours = 6;
        s.lagHours = 4;
        s.cyclesToTry = 6;
        s.fileUrl = [member, monthly] (const GfsData::Run& run, int, const std::string& file) {
            const std::string base = bucket + "cfs." + run.date + "/" + run.cycle + "/";
            const auto m = member2(member);
            if (monthly) {
                const auto dot = file.find('.');
                return base + "monthly_grib_" + m + "/" + file.substr(0, dot) + "." + m + "." + run.date + run.cycle + "." + file.substr(dot + 1) + ".avrg.grib.grb2";
            }
            return base + "time_grib_" + m + "/" + file + "." + m + "." + run.date + run.cycle + ".daily.grb2";
        };
        s.fileOf = [monthly] (const GfsData::Want& want) {
            if (monthly) {
                return want.stat + "." + want.key;   // family "." yyyymm
            }
            return want.stat;                         // the 6 hourly file's stem
        };
        return s;
    }
}

const std::vector<UtilityCfs::Variable>& UtilityCfs::variables() {
    static const std::vector<Variable> list{
        {"t2m", "2 m temperature", "TMP", "2 m above ground", "tmp2m", "flxf", "deg C", 1.0, 0.0},
        {"tmax", "2 m highest temperature", "TMAX", "2 m above ground", "tmax", "flxf", "deg C", 1.0, 0.0},
        {"tmin", "2 m lowest temperature", "TMIN", "2 m above ground", "tmin", "flxf", "deg C", 1.0, 0.0},
        {"precip", "precipitation rate", "PRATE", "surface", "prate", "flxf", "mm/day", 86400.0, 0.0},
        {"z500", "500 mb height", "HGT", "500 mb", "z500", "pgbf", "gpm", 1.0, 0.0},
        {"mslp", "sea level pressure", "PRMSL", "mean sea level", "prmsl", "pgbf", "hPa", 0.01, 0.0},
        {"pwat", "precipitable water", "PWAT", "entire atmosphere (considered as a single layer)", "pwat", "flxf", "mm", 1.0, 0.0},
        {"t850", "850 mb temperature", "TMP", "850 mb", "t850", "pgbf", "deg C", 1.0, 0.0},
    };
    return list;
}

const UtilityCfs::Variable * UtilityCfs::find(const std::string& key) {
    for (const auto& v : variables()) {
        if (v.key == key) {
            return &v;
        }
    }
    return nullptr;
}

bool UtilityCfs::latestRun(Run& run) {
    if (!GfsData{config(), source(4, false)}.latestRun(run)) {   // (the fourth member is the last to be posted: a run with it is a whole one)
        return false;
    }
    // the monthly means come after the 6 hourly files: a run whose first whole month is not there yet is the one before (the later months may still be arriving)
    for (int back = 0; back < 4; back++) {
        const auto t = runTime(run).addSecs(-static_cast<qint64>(back) * 6 * 3600);
        const Run candidate{t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
        const auto months = UtilityCfs::months(candidate);
        const auto url = source(4, true).fileUrl(candidate, 0, "flxf." + months[1]) + ".idx";
        if (URL::getBytesRangeManaged(url, 0, 200).startsWith("1:0:d=")) {
            run = candidate;
            return true;
        }
    }
    return false;
}

bool UtilityCfs::daily(const Run& run, int member, const Variable& variable, int forecastHour, GfsGrid::Grid& out, std::string& error) {
    GfsData::Want want;
    want.key = variable.key;
    want.variable = variable.variable;
    want.level = variable.level;
    want.forecast = std::to_string(forecastHour) + " hour fcst";
    want.stat = variable.dailyFile;
    std::map<std::string, GfsGrid::Grid> got;
    if (!GfsData{config(), source(member, false)}.load(run, forecastHour, {want}, got, error) || got.empty()) {
        return false;
    }
    out = got.begin()->second;
    for (auto& v : out.values) {
        v = static_cast<float>(v * variable.scale + variable.offset);
    }
    return true;
}

bool UtilityCfs::monthly(const Run& run, int member, const Variable& variable, const std::string& yyyymm, GfsGrid::Grid& out, std::string& error) {
    GfsData::Want want;
    want.key = yyyymm;
    want.variable = variable.variable;
    want.level = variable.level;
    want.stat = variable.family;
    std::map<std::string, GfsGrid::Grid> got;
    if (!GfsData{config(), source(member, true)}.load(run, 0, {want}, got, error) || got.empty()) {
        return false;
    }
    out = got.begin()->second;
    for (auto& v : out.values) {
        v = static_cast<float>(v * variable.scale + variable.offset);
    }
    return true;
}

bool UtilityCfs::pointSeries(const Run& run, int member, const Variable& variable, const std::vector<int>& hours, double lat, double lon, std::vector<float>& values, std::string& error) {
    values.assign(hours.size(), std::nanf(""));
    const GfsData data{config(), source(member, false)};
    bool any = false;
    for (size_t first = 0; first < hours.size(); first += 12) {   // twelve fields to a call: one index read for them all
        std::vector<GfsData::Need> needs;
        for (size_t i = first; i < std::min(hours.size(), first + 12); i++) {
            GfsData::Want want;
            want.key = std::to_string(i);
            want.variable = variable.variable;
            want.level = variable.level;
            want.forecast = std::to_string(hours[i]) + " hour fcst";
            want.stat = variable.dailyFile;
            needs.push_back({hours[i], want});
        }
        std::map<std::string, GfsGrid::Grid> got;
        std::string message;
        if (!data.load(run, needs, got, message)) {
            error = message;
        }
        for (const auto& [key, grid] : got) {
            const float v = grid.sample(lon > 180.0 ? lon - 360.0 : lon, lat);
            if (!std::isnan(v)) {
                values[static_cast<size_t>(std::stoi(key))] = static_cast<float>(v * variable.scale + variable.offset);
                any = true;
            }
        }
    }
    return any;
}

bool UtilityCfs::pointMonthly(const Run& run, int member, const Variable& variable, const std::vector<std::string>& months, double lat, double lon, std::vector<float>& values, std::string& error) {
    values.assign(months.size(), std::nanf(""));
    bool any = false;
    for (size_t i = 0; i < months.size(); i++) {
        GfsGrid::Grid g;
        std::string message;
        if (monthly(run, member, variable, months[i], g, message)) {
            const float v = g.sample(lon > 180.0 ? lon - 360.0 : lon, lat);
            if (!std::isnan(v)) {
                values[i] = v;
                any = true;
            }
        } else {
            error = message;
        }
    }
    return any;
}

std::vector<std::string> UtilityCfs::months(const Run& run) {
    std::vector<std::string> out;
    const QDate start = QDate::fromString(QString::fromStdString(run.date), "yyyyMMdd");
    for (int i = 0; i < 10; i++) {
        out.push_back(start.addMonths(i).toString("yyyyMM").toStdString());
    }
    return out;
}

QDateTime UtilityCfs::runTime(const Run& run) {
    return QDateTime{QDate::fromString(QString::fromStdString(run.date), "yyyyMMdd"), QTime{std::stoi(run.cycle), 0}, QTimeZone::utc()};
}

std::string UtilityCfs::runText(const Run& run) {
    return run.date.substr(0, 4) + "-" + run.date.substr(4, 2) + "-" + run.date.substr(6, 2) + " " + run.cycle + "Z";
}

std::vector<UtilityCfs::Run> UtilityCfs::recentRuns(const Run& newest, int count, int stepHours) {
    std::vector<Run> out;
    const auto start = runTime(newest);
    for (int i = 0; i < count; i++) {
        const auto t = start.addSecs(-static_cast<qint64>(i) * stepHours * 3600);
        out.push_back({t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()});
    }
    return out;
}

const std::vector<UtilityCfs::Variable>& UtilityCfs::gfsVariables() {
    static const std::vector<Variable> list{
        {"t2m", "2 m temperature", "TMP", "2 m above ground", "", "", "deg C", 1.0, 0.0},
        {"dpt", "2 m dew point", "DPT", "2 m above ground", "", "", "deg C", 1.0, 0.0},
        {"mslp", "sea level pressure", "PRMSL", "mean sea level", "", "", "hPa", 0.01, 0.0},
        {"z500", "500 mb height", "HGT", "500 mb", "", "", "gpm", 1.0, 0.0},
        {"t850", "850 mb temperature", "TMP", "850 mb", "", "", "deg C", 1.0, 0.0},
        {"pwat", "precipitable water", "PWAT", "entire atmosphere (considered as a single layer)", "", "", "mm", 1.0, 0.0},
    };
    return list;
}

bool UtilityCfs::gfsLatestRun(Run& run) {
    const auto * def = GfsModels::find("GFS");
    return def != nullptr && GfsData{config(), def->source("")}.latestRun(run);
}

bool UtilityCfs::gfsPointSeries(const Run& run, const Variable& variable, const std::vector<int>& hours, double lat, double lon, std::vector<float>& values, std::string& error) {
    values.assign(hours.size(), std::nanf(""));
    const auto * def = GfsModels::find("GFS");
    if (def == nullptr) {
        error = "no GFS model";
        return false;
    }
    GfsData data{config(), def->source("")};
    GfsData::Box box;
    box.west = std::floor(lon) - 2.0;
    box.east = std::floor(lon) + 3.0;
    box.south = std::max(-90.0, std::floor(lat) - 2.0);
    box.north = std::min(90.0, std::floor(lat) + 3.0);
    data.setBox(box);
    bool any = false;
    for (size_t first = 0; first < hours.size(); first += 8) {
        std::vector<GfsData::Need> needs;
        for (size_t i = first; i < std::min(hours.size(), first + 8); i++) {
            GfsData::Want want;
            want.key = std::to_string(i);
            want.variable = variable.variable;
            want.level = variable.level;
            needs.push_back({hours[i], want});
        }
        std::map<std::string, GfsGrid::Grid> got;
        std::string message;
        for (int attempt = 0; attempt < 3 && got.size() < needs.size(); attempt++) {   // a busy server drops some: ask again for what is missing
            std::vector<GfsData::Need> still;
            for (const auto& need : needs) {
                if (got.count(need.want.key) == 0) {
                    still.push_back(need);
                }
            }
            std::map<std::string, GfsGrid::Grid> more;
            if (!data.load(run, still, more, message)) {
                error = message;
            }
            got.insert(more.begin(), more.end());
        }
        for (const auto& [key, grid] : got) {
            const float v = grid.sample(lon, lat);
            if (!std::isnan(v)) {
                values[static_cast<size_t>(std::stoi(key))] = static_cast<float>(v * variable.scale + variable.offset);
                any = true;
            }
        }
    }
    return any;
}

namespace {
    // the calibration climatologies: one file for each start date, cycle, lead month and statistic
    GfsData::Source climateSource(int baseline) {
        GfsData::Source s;
        s.id = "CFSNORMAL" + std::to_string(baseline);
        s.label = "NOAA/NCEP CFSv2 reforecast climatology";
        s.warp = {true, 1.0, -180.0, -90.0, 180.0, 90.0};
        s.indexReplaces = ".grb2";
        s.indexSuffix = ".inv";
        s.fileUrl = [baseline] (const GfsData::Run& run, int, const std::string& file) {
            // file: "<family>.<lead>.<stat>" ("flxf.l01.fclm")
            const auto first = file.find('.');
            const auto family = file.substr(0, first), rest = file.substr(first + 1);
            const auto mm = run.date.substr(4, 2), dd = run.date.substr(6, 2);
            return std::string{"https://www.ncei.noaa.gov/oa/prod-cfs-reforecast/calibration-climatologies/"} + (family == "flxf" ? "flux" : "by-pressure-level") + "-" + std::to_string(baseline) + "-2010/" + mm + "/" + family +
                   "." + mm + "." + dd + "." + run.cycle + "." + rest + "." + std::to_string(baseline) + ".2010.grb2";
        };
        s.fileOf = [] (const GfsData::Want& want) { return want.stat; };
        s.probeFile = "flxf.l01.fclm";
        return s;
    }
}

bool UtilityCfs::climatologyOn(const Variable& variable, const std::string& yyyymmdd, int leadMonth, bool deviation, int baseline, GfsGrid::Grid& out, std::string& error) {
    if (leadMonth < 1 || leadMonth > 9) {
        error = "the CFS normal covers lead months 1 to 9";
        return false;
    }
    const std::string base = baseline == 1982 ? "1982" : "1999";
    const PermanentCache store{"climate"};
    const Run bucket{yyyymmdd, "00"};
    const auto keyFor = [&] (const Run& r) {
        return "cfs/" + base + "/" + variable.key + "_" + r.date.substr(4, 4) + "_l" + std::to_string(leadMonth) + (deviation ? "_std" : "_mean") + ".bin";
    };
    if (store.has(keyFor(bucket)) && GfsClimate::unpack(store.read(keyFor(bucket)), out)) {
        return true;
    }
    for (const int shift : {0, 1, -1, 2, -2, 3, -3}) {   // the nearest day that has a file
        const auto t = runTime(bucket).addDays(shift);
        const Run near{t.toString("yyyyMMdd").toStdString(), "00"};
        GfsData::Want want;
        want.key = "n";
        want.variable = variable.variable;
        want.level = variable.level;
        want.stat = variable.family + ".l0" + std::to_string(leadMonth) + (deviation ? ".fstd" : ".fclm");
        std::map<std::string, GfsGrid::Grid> got;
        std::string message;
        if (GfsData{config(), climateSource(baseline == 1982 ? 1982 : 1999)}.load(near, 0, {want}, got, message) && !got.empty()) {
            out = got.begin()->second;
            for (auto& v : out.values) {
                // (GDAL takes 273.15 off every temperature field, a spread of temperatures too: put it back)
                v = static_cast<float>((deviation && variable.units == "deg C" ? v + 273.15 : v) * variable.scale + (deviation ? 0.0 : variable.offset));
            }
            store.write(keyFor(bucket), GfsClimate::pack(out));
            return true;
        }
        error = message;
    }
    return false;
}


// The normal of any start date: the two stored dates around it (the 1st and the 16th of the month, 15 days apart) blended by the days between. The lead month is counted from the month of the
// start, so a stored date in the next month has a lead one less for the same calendar month; a date from the 16th on, asking for the very next month, has no such file (there is no lead 0): it
// takes the 16th's normal as it is.
bool UtilityCfs::climatology(const Variable& variable, const Run& run, int leadMonth, bool deviation, int baseline, GfsGrid::Grid& out, std::string& error) {
    if (leadMonth < 1 || leadMonth > 9) {
        error = "the CFS normal covers lead months 1 to 9";
        return false;
    }
    const auto t = runTime(run).date();
    const auto name = [] (const QDate& d) { return d.toString("yyyyMMdd").toStdString(); };
    const QDate first{t.year(), t.month(), 1}, middle{t.year(), t.month(), 16}, nextFirst = first.addMonths(1);
    if (t.day() == 1 || t.day() == 16) {
        return climatologyOn(variable, name(t.day() == 1 ? first : middle), leadMonth, deviation, baseline, out, error);
    }
    const QDate a = t.day() < 16 ? first : middle, b = t.day() < 16 ? middle : nextFirst;
    const int leadB = t.day() < 16 ? leadMonth : leadMonth - 1;
    GfsGrid::Grid ga, gb;
    if (!climatologyOn(variable, name(a), leadMonth, deviation, baseline, ga, error)) {
        return false;
    }
    if (leadB < 1 || !climatologyOn(variable, name(b), leadB, deviation, baseline, gb, error) || gb.values.size() != ga.values.size()) {
        out = ga;
        return true;
    }
    const double w = static_cast<double>(a.daysTo(t)) / static_cast<double>(a.daysTo(b));
    out = ga;
    for (size_t i = 0; i < out.values.size(); i++) {
        out.values[i] = static_cast<float>(ga.values[i] * (1.0 - w) + gb.values[i] * w);
    }
    return true;
}

bool UtilityCfs::monthlyDeparture(const Run& run, int member, const Variable& variable, const std::string& yyyymm, bool inDeviations, int baseline, GfsGrid::Grid& out, std::string& error) {
    const auto list = months(run);
    const auto at = std::find(list.begin(), list.end(), yyyymm);
    if (at == list.end() || at == list.begin()) {
        error = "the month in progress has no normal";
        return false;
    }
    GfsGrid::Grid field, normal, spread;
    if (!monthly(run, member, variable, yyyymm, field, error) || !climatology(variable, run, static_cast<int>(at - list.begin()), false, baseline, normal, error)) {
        return false;
    }
    if (inDeviations && !climatology(variable, run, static_cast<int>(at - list.begin()), true, baseline, spread, error)) {
        return false;
    }
    if (normal.values.size() != field.values.size() || (inDeviations && spread.values.size() != field.values.size())) {
        error = "the normal is on another grid";
        return false;
    }
    out = field;
    for (size_t i = 0; i < out.values.size(); i++) {
        const float d = field.values[i] - normal.values[i];
        out.values[i] = inDeviations ? (spread.values[i] > 1e-6f ? d / spread.values[i] : std::nanf("")) : d;
    }
    return true;
}

bool UtilityCfs::pointMonthlyDeparture(const Run& run, int member, const Variable& variable, const std::vector<std::string>& months, bool inDeviations, int baseline, double lat, double lon, std::vector<float>& values, std::string& error) {
    values.assign(months.size(), std::nanf(""));
    bool any = false;
    for (size_t i = 1; i < months.size(); i++) {
        GfsGrid::Grid g;
        std::string message;
        if (monthlyDeparture(run, member, variable, months[i], inDeviations, baseline, g, message)) {
            const float v = g.sample(lon > 180.0 ? lon - 360.0 : lon, lat);
            if (!std::isnan(v)) {
                values[i] = v;
                any = true;
            }
        } else {
            error = message;
        }
    }
    return any;
}
