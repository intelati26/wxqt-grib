// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsRender.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStandardPaths>
#include "common/GlobalVariables.h"
#include "gfs/GfsChart.h"
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
            auto all = Coast::lines();
            for (const char * name : {"statev2.bin", "ca.bin", "mx.bin"}) {
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

    GfsData data(const QString& folder) {
        GfsData::Config config;
        config.gdalBin = UtilityGrib::gdalBinDir();
        config.cacheFolder = folder;
        config.bytes = [] (const std::string& url, long long start, long long end) {
            return end < 0 && start == 0 ? URL::getBytes(url) : URL::getBytesRange(url, start, end < 0 ? start + 4 * 1024 * 1024 * 1024LL : end);
        };
        return GfsData{config};
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
    return model == "GFS" && GfsChart::product(param) != nullptr;
}

QByteArray GfsRender::png(Session& session, const std::string& param, const std::string& sectorId, const std::string& cycle, int hour, std::string& error) {
    const auto * product = GfsChart::product(param);
    const auto * sector = GfsChart::sector(sectorId);
    if (!product || !sector) {
        error = "no GFS chart for " + param + " " + sectorId;
        return {};
    }
    if (UtilityGrib::gdalBinDir().empty()) {
        error = "GDAL not found - install the 'gdal' package";
        return {};
    }
    const auto gfs = data(session.folder());
    // the newest published run (looked up at most every ten minutes); an earlier cycle of the screen's choice is that run stepped back to it
    static std::mutex mutex;
    static GfsData::Run latest;
    static qint64 checked = 0;
    GfsData::Run run;
    {
        std::lock_guard lock{mutex};
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (latest.date.empty() || now - checked > 600) {
            GfsData::Run found;
            if (gfs.latestRun(found)) {
                latest = found;
                checked = now;
            }
        }
        run = latest;
    }
    if (run.date.empty()) {
        error = "could not find a GFS run on NOAA's open data";
        return {};
    }
    const auto wanted = cycle.size() >= 2 ? cycle.substr(0, 2) : run.cycle;
    if (wanted != run.cycle && wanted.size() == 2 && std::isdigit(static_cast<unsigned char>(wanted[0]))) {
        auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
        t.setTimeSpec(Qt::UTC);
        for (int back = 0; back < 4 && t.toString("HH").toStdString() != wanted; back++) {
            t = t.addSecs(-6 * 3600);
        }
        run = {t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
    }
    GfsChart::Grids grids;
    if (!gfs.load(run, GfsChart::needs(*product, hour), grids, error)) {
        return {};
    }
    GfsChart::Options options;
    options.fahrenheit = UIPreferences::unitsF;
    options.lines = borders();
    const auto image = GfsChart::render(*product, *sector, grids, run, hour, options);
    if (image.isNull()) {
        error = "the GFS chart could not be drawn";
        return {};
    }
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}
