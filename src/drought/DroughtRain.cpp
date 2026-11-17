// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtRain.h"
#include <cmath>
#include <cstring>
#include <future>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTimeZone>
#include "gfs/Servers.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "util/PermanentCache.h"

namespace {
    constexpr int columns = 1400, rows = 700;
    constexpr double west = -130.0, south = 20.0, east = -60.0, north = 55.0, step = 0.05;
    constexpr uint16_t none = 65535;   // no value; the others are twentieths of a millimeter

    std::string bucket() {
        return Servers::first("mrms-aws", "https://noaa-mrms-pds.s3.amazonaws.com") + "/";
    }

    // one file's rain on the grid, as 16 bit twentieths of a millimeter ("" when it could not be had)
    std::string fetchPiece(const std::string& hours, const QDateTime& end) {
        const auto binDir = UtilityGrib::gdalBinDir();
        if (binDir.empty()) {
            return {};
        }
        const std::string product = "CONUS/MultiSensor_QPE_" + hours + "_Pass2_00.00/";
        const auto day = end.toString("yyyyMMdd").toStdString();
        const auto listing = URL::getBytes(bucket() + "?list-type=2&prefix=" + product + day + "/MRMS_MultiSensor_QPE_" + hours + "_Pass2_00.00_" + day + "-" + end.toString("HH").toStdString() + "&max-keys=1").toStdString();
        const auto key = QRegularExpression{"<Key>([^<]+)</Key>"}.match(QString::fromStdString(listing));
        if (!key.hasMatch()) {
            return {};
        }
        const auto bytes = URL::getBytes(bucket() + key.captured(1).toStdString());
        if (bytes.size() < 100) {
            return {};
        }
        QTemporaryFile gz{QDir::tempPath() + "/wxqt_rain_XXXXXX.grib2.gz"};
        if (!gz.open() || gz.write(bytes) != bytes.size()) {
            return {};
        }
        gz.flush();
        const QString rawPath = gz.fileName() + ".raw";
        QProcess warp;
        warp.start(QString::fromStdString(binDir) + "/gdalwarp", QStringList{"-q", "-overwrite", "-r", "average", "-srcnodata", "-3", "-dstnodata", "-9999", "-te", QString::number(west), QString::number(south),
                                                                                QString::number(east), QString::number(north), "-tr", QString::number(step), QString::number(step), "-of", "ENVI", "-ot", "Float32",
                                                                                "/vsigzip/" + gz.fileName(), rawPath});
        warp.waitForFinished(300000);
        QFile raw{rawPath};
        const auto data = warp.exitStatus() == QProcess::NormalExit && warp.exitCode() == 0 && raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
        raw.close();
        QFile::remove(rawPath);
        QFile::remove(rawPath + ".aux.xml");
        QFile::remove(rawPath.left(rawPath.size() - 4) + ".hdr");
        if (static_cast<size_t>(data.size()) != static_cast<size_t>(columns) * rows * 4) {
            return {};
        }
        std::string out(static_cast<size_t>(columns) * rows * 2, '\0');
        const auto * in = reinterpret_cast<const float *>(data.constData());
        auto * packed = reinterpret_cast<uint16_t *>(out.data());
        for (size_t i = 0; i < static_cast<size_t>(columns) * rows; i++) {
            const float v = in[i];
            packed[i] = (std::isnan(v) || v < 0.0f) ? none : static_cast<uint16_t>(std::min(65534.0f, std::round(v * 20.0f)));
        }
        return out;
    }

    std::string piece(const std::string& hours, const QDateTime& end) {
        const PermanentCache store{"drought"};
        const auto name = "mrms_" + hours + "_" + end.toString("yyyyMMddHH").toStdString() + ".u16";
        return store.fetch(name, name, 1000, [&] { return fetchPiece(hours, end); }).text;
    }
}

QDateTime DroughtRain::latestEnd(const QDateTime& now) {
    auto t = now.toUTC().addSecs(-3 * 3600);
    t = QDateTime{t.date(), QTime{12, 0}, QTimeZone::utc()};
    if (t > now.toUTC().addSecs(-3 * 3600)) {
        t = t.addDays(-1);
    }
    return t;
}

int DroughtRain::pieces(int days) {
    return days / 3 + days % 3;
}

DroughtRain::Grid DroughtRain::total(const QDateTime& end, int days, std::atomic<int> * done) {
    Grid grid;
    std::vector<std::pair<std::string, QDateTime>> pieces;   // 72 hour files for three days at a time, 24 hour files for the rest
    auto at = end.toUTC();
    int left = days;
    while (left >= 3) {
        pieces.emplace_back("72H", at);
        at = at.addDays(-3);
        left -= 3;
    }
    while (left > 0) {
        pieces.emplace_back("24H", at);
        at = at.addDays(-1);
        left--;
    }
    std::vector<float> sum(static_cast<size_t>(columns) * rows, 0.0f);
    std::vector<bool> missing(sum.size(), false);
    // four files at a time (a year is about 120 of them the first time)
    std::vector<std::string> fetched(pieces.size());
    for (size_t first = 0; first < pieces.size(); first += 4) {
        std::vector<std::future<std::string>> batch;
        for (size_t i = first; i < std::min(pieces.size(), first + 4); i++) {
            batch.push_back(std::async(std::launch::async, [&pieces, i, done] {
                auto data = piece(pieces[i].first, pieces[i].second);
                if (done != nullptr) {
                    ++*done;
                }
                return data;
            }));
        }
        for (size_t i = 0; i < batch.size(); i++) {
            fetched[first + i] = batch[i].get();
        }
    }
    for (size_t k = 0; k < pieces.size(); k++) {
        const auto& hours = pieces[k].first;
        const auto& when = pieces[k].second;
        const auto& data = fetched[k];
        if (data.size() != sum.size() * 2) {
            grid.error = "no MRMS " + hours + " rain total for " + when.toString("yyyy-MM-dd HH").toStdString() + "Z";
            return grid;
        }
        const auto * packed = reinterpret_cast<const uint16_t *>(data.data());
        for (size_t i = 0; i < sum.size(); i++) {
            if (packed[i] == none) {
                missing[i] = true;
            } else {
                sum[i] += packed[i] / 20.0f;
            }
        }
        grid.files++;
    }
    for (size_t i = 0; i < sum.size(); i++) {
        if (missing[i]) {
            sum[i] = std::nanf("");
        }
    }
    grid.west = west;
    grid.north = north;
    grid.step = step;
    grid.columns = columns;
    grid.rows = rows;
    grid.mm = std::move(sum);
    return grid;
}
