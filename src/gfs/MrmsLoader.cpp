// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/MrmsLoader.h"
#include <cmath>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimeZone>
#include "gfs/GfsData.h"
#include "gfs/GfsGrid.h"
#include "objects/URL.h"
#include "models/UtilityGrib.h"

namespace {
    const std::string bucket{"https://noaa-mrms-pds.s3.amazonaws.com/"};
    const std::string product{"CONUS/MergedReflectivityQCComposite_00.50/"};
    constexpr double step = 0.04;

    bool load(const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        if (want.variable != "REFC") {
            error = "MRMS has the composite reflectivity only";
            return false;
        }
        const auto binDir = UtilityGrib::gdalBinDir();
        if (binDir.empty()) {
            error = "GDAL not found - install the 'gdal' package";
            return false;
        }
        auto valid = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
        valid.setTimeZone(QTimeZone::utc());
        valid = valid.addSecs(3600LL * hour);
        const auto day = valid.toString("yyyyMMdd").toStdString();
        // the scans of that hour, listed by the bucket: the first is the one of the hour
        const auto listing = URL::getBytes(bucket + "?list-type=2&prefix=" + product + day + "/MRMS_MergedReflectivityQCComposite_00.50_" + day + "-" + valid.toString("HH").toStdString() + "&max-keys=1").toStdString();
        const auto key = QRegularExpression{"<Key>([^<]+)</Key>"}.match(QString::fromStdString(listing));
        if (!key.hasMatch()) {
            error = "MRMS has no scan for " + valid.toString("yyyy-MM-dd HH").toStdString() + "Z";
            return false;
        }
        const auto bytes = URL::getBytes(bucket + key.captured(1).toStdString());
        if (bytes.size() < 100) {
            error = "could not download the MRMS scan " + key.captured(1).toStdString();
            return false;
        }
        QTemporaryFile gz{QDir::tempPath() + "/wxqt_mrms_XXXXXX.grib2.gz"};
        if (!gz.open() || gz.write(bytes) != bytes.size()) {
            error = "could not keep the MRMS scan";
            return false;
        }
        gz.flush();
        const QString rawPath = gz.fileName() + ".raw", hdrPath = gz.fileName() + ".hdr";
        const auto cleanup = [&] {
            QFile::remove(rawPath);
            QFile::remove(hdrPath);
            QFile::remove(rawPath + ".aux.xml");
        };
        QProcess warp;
        warp.start(QString::fromStdString(binDir) + "/gdalwarp", QStringList{"-q", "-overwrite", "-r", "max", "-srcnodata", "-999", "-dstnodata", "-9999", "-tr", QString::number(step), QString::number(step), "-of", "ENVI", "-ot", "Float32", "/vsigzip/" + gz.fileName(), rawPath});
        warp.waitForFinished(300000);
        if (warp.exitStatus() != QProcess::NormalExit || warp.exitCode() != 0) {
            error = "gdalwarp failed on the MRMS scan: " + warp.readAllStandardError().trimmed().toStdString();
            cleanup();
            return false;
        }
        QFile header{hdrPath};
        const auto text = header.open(QIODevice::ReadOnly) ? QString::fromUtf8(header.readAll()) : QString{};
        const auto number = [&text] (const QString& name) {
            const auto m = QRegularExpression{"\\b" + name + "\\s*=\\s*([-0-9.]+)"}.match(text);
            return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
        };
        const auto info = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(text);
        out = GfsGrid::Grid{};
        out.columns = static_cast<int>(number("samples"));
        out.rows = static_cast<int>(number("lines"));
        QFile raw{rawPath};
        const auto data = raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
        cleanup();
        if (out.columns < 2 || out.rows < 2 || !info.hasMatch() || static_cast<size_t>(data.size()) != static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows) * 4) {
            error = "the MRMS scan is not on a plain latitude / longitude grid";
            return false;
        }
        out.step = info.captured(3).toDouble();
        out.lon0 = info.captured(1).toDouble() + out.step / 2.0;
        out.lat0 = info.captured(2).toDouble() - out.step / 2.0;
        out.values.resize(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows));
        std::memcpy(out.values.data(), data.constData(), static_cast<size_t>(data.size()));
        for (auto& v : out.values) {   // no data (-999, made -9999 by the warp): nothing to compare; "no echo" (-99): 0 dBZ, which is what a model's empty sky is
            if (v < -9000.0f) {
                v = std::nanf("");
            } else if (v <= -50.0f) {
                v = 0.0f;
            }
        }
        return true;
    }
}

void MrmsLoader::registerLoader() {
    GfsData::setLoader("MRMS", load);
}
