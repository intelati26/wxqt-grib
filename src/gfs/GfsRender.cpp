// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsRender.h"
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <QBuffer>
#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStandardPaths>
#include "common/GlobalVariables.h"
#include "gfs/GfsChart.h"
#include "gfs/GfsClimate.h"
#include "gfs/GfsData.h"
#include "hurricane/Coast.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "settings/UIPreferences.h"
#include "util/UtilityIO.h"

namespace {
    // the coastlines of the basins plus the states, Canada and Mexico (line segments in the radar screen's resources: latitude, west longitude)
    const std::vector<std::vector<std::pair<float, float>>>& borders() {
        static const auto lines = [] {
            auto all = Coast::worldLines();
            for (const char * name : {"statev2.bin"}) {   // the state lines; the world file has the countries and the coasts
                const auto raw = UtilityIO::readBinaryFileFromResource(GlobalVariables::resDir + name);
                const auto floatAt = [&raw] (int offset) {
                    const unsigned char b[4]{static_cast<unsigned char>(raw[offset + 3]), static_cast<unsigned char>(raw[offset + 2]),
                                             static_cast<unsigned char>(raw[offset + 1]), static_cast<unsigned char>(raw[offset])};
                    float value = 0.0f;
                    std::memcpy(&value, b, 4);
                    return value;
                };
                for (int i = 0; i + 15 < static_cast<int>(raw.size()); i += 16) {
                    const float lat1 = floatAt(i), lon1 = floatAt(i + 4), lat2 = floatAt(i + 8), lon2 = floatAt(i + 12);
                    if ((lat1 == lat2 && lon1 == lon2) || lat1 < 5.0f || lat1 > 85.0f || lat2 < 5.0f || lat2 > 85.0f) {
                        continue;
                    }
                    all.push_back({{-lon1, lat1}, {-lon2, lat2}});
                }
            }
            return all;
        }();
        return lines;
    }

    bool isHafs(const std::string& model) {
        return model == "HAFSA" || model == "HAFSB";
    }

    GfsData::Source sourceOf(const std::string& model, const std::string& storm = "") {
        if (isHafs(model)) {
            return GfsData::hafs(model, storm);
        }
        return model == "NBM" ? GfsData::nbm() : model == "AIGFS" ? GfsData::aigfs() : model == "GEFS" ? GfsData::gefs() : GfsData::gfs();
    }

    GfsData data(const QString& folder, const std::string& model, const std::string& storm = "") {
        GfsData::Config config;
        config.gdalBin = UtilityGrib::gdalBinDir();
        config.cacheFolder = folder;
        config.bytes = [] (const std::string& url, long long start, long long end) {
            return end < 0 && start == 0 ? URL::getBytes(url) : URL::getBytesRange(url, start, end < 0 ? start + 4 * 1024 * 1024 * 1024LL : end);
        };
        return GfsData{config, sourceOf(model, storm)};
    }

    // the newest published run of a model, looked up at most every ten minutes
    bool newestRun(const GfsData& gfs, GfsData::Run& run) {
        static std::mutex mutex;
        static std::map<std::string, std::pair<GfsData::Run, qint64>> known;
        std::lock_guard lock{mutex};
        auto& entry = known[gfs.model().id];
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (entry.first.date.empty() || now - entry.second > 600) {
            GfsData::Run found;
            if (gfs.latestRun(found)) {
                entry = {found, now};
            }
        }
        run = entry.first;
        return !run.date.empty();
    }
}

GfsRender::Session::Session() {
    // the first of these screens removes what the earlier versions left in the cache folder (decoded grids that were never cleared)
    static std::once_flag once;
    std::call_once(once, [] { QDir{QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/gfs"}.removeRecursively(); });
    const auto base = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/wxqt_gfs_session_XXXXXX";
    QTemporaryDir dir{base};
    dir.setAutoRemove(false);   // removed in the destructor, so the path outlives this scope
    path = dir.path();
}

GfsRender::Session::~Session() {
    QDir{path}.removeRecursively();
}

QString GfsRender::Session::folder() const {
    return path;
}

QString GfsRender::Session::partialGrib(const std::string& cycleRun, int hour) const {
    const auto file = path + "/gfs." + QString::fromStdString(cycleRun) + ".f" + QString::number(hour).rightJustified(3, '0') + ".partial.grib2";
    return QFileInfo::exists(file) ? file : QString{};
}

bool GfsRender::handles(const std::string& model, const std::string& param) {
    return (model == "GFS" || model == "NBM" || model == "AIGFS" || model == "GEFS" || isHafs(model)) && GfsChart::product(param, model) != nullptr;
}

bool GfsRender::latestCycle(const std::string& model, std::string& cycle, const std::string& storm) {
    const auto gfs = data({}, model, storm);
    GfsData::Run run;
    if (!newestRun(gfs, run)) {
        return false;
    }
    cycle = run.cycle + "Z";
    return true;
}

QByteArray GfsRender::png(Session& session, const std::string& model, const std::string& param, const std::string& sectorId, const std::string& cycle, int hour, const std::vector<std::string>& overlays, std::string& error) {
    const auto * base = GfsChart::product(param, model);
    auto sector = GfsChart::sector(sectorId);
    GfsChart::Sector storm;   // the hurricane model's grid follows the storm: the chart is its whole grid
    const std::string stormId = isHafs(model) ? sectorId : std::string{};
    if (isHafs(model)) {
        sector = &storm;
    }
    const auto composed = base ? GfsChart::compose(*base, overlays) : GfsChart::Product{};   // the chart with what was ticked onto it
    const auto * product = base ? &composed : nullptr;
    if (!product || !sector) {
        error = "no " + model + " chart for " + param + " " + sectorId;
        return {};
    }
    if (UtilityGrib::gdalBinDir().empty()) {
        error = "GDAL not found - install the 'gdal' package";
        return {};
    }
    const auto gfs = data(session.folder(), model, stormId);
    // the newest published run; an earlier cycle of the screen's choice is that run stepped back to it
    GfsData::Run run;
    if (!newestRun(gfs, run)) {
        error = "could not find a " + model + " run on NOAA's open data";
        return {};
    }
    const auto wanted = cycle.size() >= 2 ? cycle.substr(0, 2) : run.cycle;
    if (wanted != run.cycle && wanted.size() == 2 && std::isdigit(static_cast<unsigned char>(wanted[0]))) {
        auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
        t.setTimeSpec(Qt::UTC);
        const int step = gfs.model().cycleHours;
        for (int back = 0; back < 24 / step && t.toString("HH").toStdString() != wanted; back++) {
            t = t.addSecs(-static_cast<qint64>(step) * 3600);
        }
        run = {t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
    }
    GfsChart::Grids grids;
    bool loaded = gfs.load(run, GfsChart::needs(*product, hour), grids, error);
    if (!loaded) {   // the same run's 1 hour pieces, if the product has them (the first 36 hours)
        const auto pieces = GfsChart::fallbackNeeds(*product, hour);
        std::string again;
        grids.clear();
        loaded = !pieces.empty() && gfs.load(run, pieces, grids, again);
        if (loaded) {
            error.clear();
        }
    }
    if (!loaded) {
        // the blend's runs that are not at 00, 06, 12 or 18Z carry only the 1 hour amounts and fewer fields: the newest of the main runs has the rest, with the forecast hour moved to
        // keep the same valid time
        if (isHafs(model)) {   // the waves come out after the rest: the cycle before, six hours further on, has the same valid time
            auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(-6 * 3600);
            const GfsData::Run earlier{t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
            grids.clear();
            std::string again;
            if (!gfs.load(earlier, GfsChart::needs(*product, hour + 6), grids, again)) {
                return {};   // the first reason stands
            }
            run = earlier;
            hour += 6;
            error.clear();
        } else {
        const int cycle = std::atoi(run.cycle.c_str());
        if (model != "NBM" || cycle % 6 == 0) {
            return {};
        }
        const int earlier = cycle % 6;
        auto alternative = run;
        alternative.cycle = (cycle - earlier < 10 ? "0" : "") + std::to_string(cycle - earlier);
        grids.clear();
        std::string again;
        if (!gfs.load(alternative, GfsChart::needs(*product, hour + earlier), grids, again)) {
            return {};   // the first reason stands
        }
        run = alternative;
        hour += earlier;
        error.clear();
        }
    }
    if (isHafs(model)) {   // the extent of the grid of the first field
        for (const auto& need : GfsChart::needs(*product, hour)) {
            const auto found = grids.find(need.want.key);
            if (found != grids.end() && !found->second.empty() && need.want.stat != "ww3") {
                const auto& g = found->second;
                storm = GfsChart::gridSector(sectorId, g);
                break;
            }
        }
        if (storm.id.empty() && !grids.empty()) {   // only the wave file: its own grid
            const auto& g = grids.begin()->second;
            storm = GfsChart::gridSector(sectorId, g);
        }
    }
    GfsChart::Options options;
    static const GfsClimate climate{UtilityGrib::gdalBinDir()};
    options.climate = &climate;
    options.fahrenheit = UIPreferences::unitsF;
    options.lines = borders();
    const auto image = GfsChart::render(*product, *sector, grids, run, hour, options);
    if (image.isNull()) {
        error = product->derive ? "the chart could not be drawn (some charts need a connection the first time, for the climatology, or the record is not in this run at this hour)" : "the " + model + " chart could not be drawn";
        return {};
    }
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

std::vector<std::string> GfsRender::hafsStorms(const std::string& model, std::string& cycle) {
    std::vector<std::string> storms;
    if (!isHafs(model)) {
        return storms;
    }
    const std::string letter = model == "HAFSB" ? "b" : "a";
    auto t = QDateTime::currentDateTimeUtc();
    t = QDateTime{t.date(), QTime{t.time().hour() / 6 * 6, 0}, Qt::UTC};
    for (int back = 0; back < 6 && storms.empty(); back++, t = t.addSecs(-6 * 3600)) {   // the newest cycle that has any
        const auto folder = "https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfs" + letter + "." + t.toString("yyyyMMdd").toStdString() + "/" + t.toString("HH").toStdString() + "/";
        const auto text = QString::fromUtf8(URL::getBytes(folder));
        std::set<std::string> found;
        const QRegularExpression pattern{"href=\"([0-9]{2}[lecwsa])\\." + QString::fromStdString(t.toString("yyyyMMddHH").toStdString()) + "\\.hfs" + QString::fromStdString(letter) + "\\.storm\\.atm\\.f000\\.grb2\""};
        for (auto it = pattern.globalMatch(text); it.hasNext();) {
            found.insert(it.next().captured(1).toStdString());
        }
        storms.assign(found.begin(), found.end());
        cycle = t.toString("HH").toStdString() + "Z";
    }
    return storms;
}
