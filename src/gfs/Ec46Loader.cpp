// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/Ec46Loader.h"
#include <cmath>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "cfs/UtilityOpenMeteoSeasonal.h"
#include "gfs/GfsData.h"
#include "gfs/GfsGrid.h"
#include "gfs/OmFile.h"
#include "gfs/OmGrid.h"
#include "objects/URL.h"

namespace {
    // the world in coarse points (the service allows about 600 points a minute and 5,000 an hour, so the world takes a minute and a half a field), with the United States in finer ones inside it
    constexpr double worldWest = -180.0, worldSouth = -60.0, worldEast = 180.0, worldNorth = 80.0, worldStep = 10.0;
    constexpr double west = -135.0, south = 14.0, east = -55.0, north = 58.0, step = 4.0, fine = 0.5;

    // a coarse grid of the service's points as a GfsGrid
    GfsGrid::Grid coarseOf(const UtilityOpenMeteoSeasonal::PeriodGrid& g, size_t week) {
        GfsGrid::Grid coarse;
        coarse.columns = g.columns;
        coarse.rows = g.rows;
        coarse.lon0 = g.west;
        coarse.lat0 = g.north;
        coarse.step = g.step;
        coarse.values = g.values[week];
        return coarse;
    }

    // The weekly ensemble means from Open-Meteo's own files (open-meteo/om-file-format on their open bucket): one file of about 10 MB for each week, in which a field is read by byte ranges (a few hundred
    // kilobytes) and no call to the API is spent. The grid is ECMWF's O320 reduced Gaussian grid, interpolated onto the same half degree grid as the API's points are spread onto.
    constexpr const char * bucket = "https://openmeteo.s3.amazonaws.com/data_spatial/ecmwf_ec46_weekly/";

    std::string runFolder(const QDate& day) {
        return bucket + day.toString("yyyy/MM/dd").toStdString() + "/0000Z/";
    }

    // the newest of the last few days' runs (one each day at 00Z) that has published its valid times, and those times
    bool runFiles(const GfsData::Run& run, std::string& folder, std::vector<std::string>& validTimes) {
        auto day = QDate::fromString(QString::fromStdString(run.date), "yyyyMMdd");
        if (!day.isValid()) {
            day = QDate::currentDate();
        }
        for (int back = 0; back < 4; back++) {
            const auto candidate = runFolder(day.addDays(-back));
            const auto meta = QJsonDocument::fromJson(URL::getBytesManaged(candidate + "meta.json")).object();
            if (!meta.value("completed").toBool()) {
                continue;
            }
            validTimes.clear();
            for (const auto& t : meta.value("valid_times").toArray()) {
                validTimes.push_back(t.toString().left(10).toStdString());   // "2026-11-09T00:00Z"
            }
            if (!validTimes.empty()) {
                folder = candidate;
                return true;
            }
        }
        return false;
    }

    bool loadWeekFromFiles(const GfsData::Run& run, int forecastHour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        std::string folder;
        std::vector<std::string> times;
        if (!runFiles(run, folder, times)) {
            error = "no EC46 run in the last days";
            return false;
        }
        const auto week = static_cast<size_t>(std::max(0, forecastHour) / 168);
        if (week >= times.size()) {
            error = "the EC46 forecast has no week " + std::to_string(week + 1) + " yet";
            return false;
        }
        const auto url = folder + times[week] + "T0000.om";
        OmFile::Io io;
        io.merge = 262144;
        io.max = 8u << 20;
        io.range = [&url] (uint64_t offset, uint64_t size, std::string& bytes) {
            if (size == 0) {
                bytes.clear();
                return true;
            }
            const auto got = URL::getBytesRangeManaged(url, static_cast<long long>(offset), static_cast<long long>(offset + size - 1));
            bytes.assign(got.constData(), static_cast<size_t>(got.size()));
            return static_cast<uint64_t>(got.size()) >= size;
        };
        io.tail = [&url] (uint64_t size, std::string& bytes) {
            const auto got = URL::getBytesSuffixManaged(url, static_cast<long long>(size));
            bytes.assign(got.constData(), static_cast<size_t>(got.size()));
            return static_cast<uint64_t>(got.size()) >= size;
        };
        OmFile file;
        if (!file.open(io, error)) {
            return false;
        }
        std::vector<float> values;
        if (!file.has(want.variable)) {
            error = "the EC46 file has no " + want.variable;
            return false;
        }
        if (!file.readAll(want.variable, values, error)) {
            return false;
        }
        return OmGrid::gaussianToRegular(values, 320, fine, worldWest, worldSouth, worldEast, worldNorth, out, error);
    }

    // a forecast "hour" is a period of the model: 168 hours a week for the EC46, 720 a month for the SEAS5
    bool loadPeriod(bool weekly, int forecastHour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        const auto week = static_cast<size_t>(std::max(0, forecastHour) / (weekly ? 168 : 720));
        UtilityOpenMeteoSeasonal::PeriodGrid world;
        if (!UtilityOpenMeteoSeasonal::periodGrid(weekly, want.variable, worldWest, worldSouth, worldEast, worldNorth, worldStep, world, error)) {
            return false;
        }
        if (week >= world.starts.size()) {
            error = std::string{"the "} + (weekly ? "EC46 forecast has no week " : "SEAS5 forecast has no month ") + std::to_string(week + 1) + " yet";
            return false;
        }
        const auto coarseWorld = coarseOf(world, week);
        UtilityOpenMeteoSeasonal::PeriodGrid us;
        std::string ignored;
        const bool haveUs = UtilityOpenMeteoSeasonal::periodGrid(weekly, want.variable, west, south, east, north, step, us, ignored) && week < us.starts.size();   // (the world alone when the service's minute is spent)
        GfsGrid::Grid coarseUs;
        if (haveUs) {
            coarseUs = coarseOf(us, week);
        }
        // smooth: the weekly mean of an ensemble at a 36 km model varies slowly, and the chart colors the values between the points
        out.columns = static_cast<int>(std::lround((worldEast - worldWest) / fine)) + 1;
        out.rows = static_cast<int>(std::lround((worldNorth - worldSouth) / fine)) + 1;
        out.lon0 = worldWest;
        out.lat0 = worldNorth;
        out.step = fine;
        out.values.assign(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows), std::nanf(""));
        for (int r = 0; r < out.rows; r++) {
            const double lat = worldNorth - r * fine;
            for (int c = 0; c < out.columns; c++) {
                const double lon = worldWest + c * fine;
                const bool inUs = haveUs && lat >= south && lat <= north && lon >= west && lon <= east;
                const auto& source = inUs ? coarseUs : coarseWorld;
                const double la = std::clamp(lat, source.lat0 - (source.rows - 1) * source.step, source.lat0);
                const double lo = std::clamp(lon, source.lon0, source.lon0 + (source.columns - 1) * source.step);
                out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = source.sampleCubic(lo, la);
            }
        }
        return true;
    }

    // the files first; the API when they have no such run, week or field (its points are the same ensemble mean)
    bool loadWeek(const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        std::string fromFiles;
        if (loadWeekFromFiles(run, hour, want, out, fromFiles)) {
            return true;
        }
        return loadPeriod(true, hour, want, out, error);
    }
    bool loadMonth(const GfsData::Run&, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) { return loadPeriod(false, hour, want, out, error); }
}

void Ec46Loader::registerLoader() {
    GfsData::setLoader("EC46", loadWeek);
    GfsData::setLoader("SEAS5", loadMonth);
}
