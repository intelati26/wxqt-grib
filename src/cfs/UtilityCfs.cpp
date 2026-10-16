// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/UtilityCfs.h"
#include <QDate>
#include <QDateTime>
#include "gfs/GfsCache.h"
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
        const auto t = QDateTime::fromString(QString::fromStdString(run.date + run.cycle), "yyyyMMddHH").addSecs(-static_cast<qint64>(back) * 6 * 3600);
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

std::vector<std::string> UtilityCfs::months(const Run& run) {
    std::vector<std::string> out;
    const QDate start = QDate::fromString(QString::fromStdString(run.date), "yyyyMMdd");
    for (int i = 0; i < 10; i++) {
        out.push_back(start.addMonths(i).toString("yyyyMM").toStdString());
    }
    return out;
}

std::string UtilityCfs::runText(const Run& run) {
    return run.date.substr(0, 4) + "-" + run.date.substr(4, 2) + "-" + run.date.substr(6, 2) + " " + run.cycle + "Z";
}
