// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mrms/UtilityMrms.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTimeZone>
#include "models/UtilityGrib.h"
#include "objects/URL.h"

namespace {
    const string site{"https://mrms.ncep.noaa.gov/2D/"};
    vector<UtilityMrms::Stop> reflectivityStops() {
        return {{5, 4, 233, 231}, {10, 1, 159, 244}, {15, 3, 0, 244}, {20, 2, 253, 2}, {25, 1, 197, 1}, {30, 0, 142, 0},
                {35, 253, 248, 2}, {40, 229, 188, 0}, {45, 253, 149, 0}, {50, 253, 0, 0}, {55, 212, 0, 0}, {60, 188, 0, 0},
                {65, 248, 0, 253}, {70, 152, 84, 198}, {75, 253, 253, 253}};
    }

    vector<UtilityMrms::Stop> rainStops() {   // mm/h and mm
        return {{0.1, 150, 200, 150}, {1, 0, 200, 0}, {5, 255, 255, 0}, {10, 255, 165, 0}, {25, 255, 0, 0},
                {50, 200, 0, 200}, {100, 255, 255, 255}};
    }

    // decoded scans live in their own temp folder; anything older than a few hours is dropped when a scan list is read
    string pathFor(const string& name) {
        const auto dir = QDir::tempPath() + "/wxqt_mrms";
        QDir{}.mkpath(dir);
        return (dir + "/").toStdString() + name;
    }

    void pruneCache() {
        const QDir dir{QDir::tempPath() + "/wxqt_mrms"};
        for (const auto& info : dir.entryInfoList(QDir::Files)) {
            if (info.lastModified().secsTo(QDateTime::currentDateTime()) > 6 * 3600) {
                QFile::remove(info.absoluteFilePath());
            }
        }
    }
}

const vector<UtilityMrms::Product>& UtilityMrms::products() {
    using P = Product;
    static const vector<P> all{
        P{"MergedReflectivityQCComposite", "Composite reflectivity", "dBZ", 5.0, 75.0, reflectivityStops()},
        P{"ReflectivityAtLowestAltitude", "Reflectivity at lowest altitude", "dBZ", 5.0, 75.0, reflectivityStops()},
        P{"MESH", "Hail size (MESH)", "mm", 1.0, 102.0,
          {{1, 120, 200, 255}, {10, 0, 255, 0}, {19, 255, 255, 0}, {25, 255, 165, 0}, {32, 255, 0, 0}, {51, 200, 0, 100},
           {76, 150, 0, 200}, {102, 255, 255, 255}}},
        P{"MESH_Max_60min", "Hail size, 1-hour max", "mm", 1.0, 102.0,
          {{1, 120, 200, 255}, {10, 0, 255, 0}, {19, 255, 255, 0}, {25, 255, 165, 0}, {32, 255, 0, 0}, {51, 200, 0, 100},
           {76, 150, 0, 200}, {102, 255, 255, 255}}},
        P{"MESH_Max_1440min", "Hail size, 24-hour max", "mm", 1.0, 102.0,
          {{1, 120, 200, 255}, {10, 0, 255, 0}, {19, 255, 255, 0}, {25, 255, 165, 0}, {32, 255, 0, 0}, {51, 200, 0, 100},
           {76, 150, 0, 200}, {102, 255, 255, 255}}},
        P{"RotationTrack60min", "Rotation track, 1 hour", "1/s", 0.002, 0.03,
          {{0.002, 120, 200, 255}, {0.006, 0, 220, 0}, {0.010, 255, 255, 0}, {0.015, 255, 165, 0}, {0.020, 255, 0, 0},
           {0.030, 255, 0, 255}}},
        P{"RotationTrack1440min", "Rotation track, 24 hours", "1/s", 0.002, 0.03,
          {{0.002, 120, 200, 255}, {0.006, 0, 220, 0}, {0.010, 255, 255, 0}, {0.015, 255, 165, 0}, {0.020, 255, 0, 0},
           {0.030, 255, 0, 255}}},
        P{"PrecipRate", "Precipitation rate", "mm/h", 0.05, 100.0, rainStops()},
        P{"MultiSensor_QPE_01H_Pass2", "Rain, 1 hour (multi-sensor)", "mm", 0.1, 100.0, rainStops()},
        P{"MultiSensor_QPE_03H_Pass2", "Rain, 3 hours (multi-sensor)", "mm", 0.1, 100.0, rainStops()},
        P{"MultiSensor_QPE_06H_Pass2", "Rain, 6 hours (multi-sensor)", "mm", 0.1, 100.0, rainStops()},
        P{"MultiSensor_QPE_24H_Pass2", "Rain, 24 hours (multi-sensor)", "mm", 0.1, 100.0, rainStops()},
        P{"VIL", "Vertically integrated liquid", "kg/m2", 5.0, 80.0,
          {{5, 120, 200, 255}, {15, 0, 200, 0}, {30, 255, 255, 0}, {45, 255, 165, 0}, {60, 255, 0, 0}, {80, 255, 0, 255}}},
        P{"EchoTop_18", "Echo top (18 dBZ)", "km", 2.0, 20.0,
          {{2, 120, 200, 255}, {6, 0, 200, 0}, {10, 255, 255, 0}, {14, 255, 165, 0}, {17, 255, 0, 0}, {20, 255, 0, 255}}},
        P{"LightningProbabilityNext30min", "Lightning probability, next 30 min", "%", 10.0, 100.0,
          {{10, 120, 200, 255}, {30, 0, 200, 0}, {50, 255, 255, 0}, {70, 255, 165, 0}, {90, 255, 0, 0}, {100, 255, 0, 255}}},
    };
    return all;
}

QVector<QRgb> UtilityMrms::colorTable(const Product& product) {
    QVector<QRgb> table(256, qRgba(0, 0, 0, 0));
    const double step = (product.hi - product.validMin) / 254.0;
    for (int index = 1; index < 256; index += 1) {
        const double value = product.validMin + (index - 1) * step;
        const auto& stops = product.stops;
        int r = stops.front().r;
        int g = stops.front().g;
        int b = stops.front().b;
        if (value >= stops.back().value) {
            r = stops.back().r;
            g = stops.back().g;
            b = stops.back().b;
        } else if (value > stops.front().value) {
            for (size_t i = 1; i < stops.size(); i += 1) {
                if (value <= stops[i].value) {
                    const double t = (value - stops[i - 1].value) / (stops[i].value - stops[i - 1].value);
                    r = static_cast<int>(stops[i - 1].r + t * (stops[i].r - stops[i - 1].r));
                    g = static_cast<int>(stops[i - 1].g + t * (stops[i].g - stops[i - 1].g));
                    b = static_cast<int>(stops[i - 1].b + t * (stops[i].b - stops[i - 1].b));
                    break;
                }
            }
        }
        table[index] = qRgba(r, g, b, 255);
    }
    return table;
}

bool UtilityMrms::scans(const Product& product, vector<Scan>& out, string& error) {
    out.clear();
    pruneCache();
    const auto listing = URL::getText(site + product.id + "/");
    if (listing.empty()) {
        error = "no answer from mrms.ncep.noaa.gov (offline, blocked, or the site is down)";
        return false;
    }
    const QRegularExpression pattern{QString::fromStdString("MRMS_" + product.id + "_[0-9.]+_(\\d{8})-(\\d{6})\\.grib2\\.gz")};
    auto matches = pattern.globalMatch(QString::fromStdString(listing));
    while (matches.hasNext()) {
        const auto match = matches.next();
        const auto utc = QDateTime::fromString(match.captured(1) + match.captured(2), "yyyyMMddhhmmss");
        if (utc.isValid()) {
            out.push_back({match.captured(0).toStdString(), QDateTime{utc.date(), utc.time(), QTimeZone::utc()}});
        }
    }
    std::sort(out.begin(), out.end(), [] (const Scan& a, const Scan& b) { return a.utc < b.utc; });
    out.erase(std::unique(out.begin(), out.end(), [] (const Scan& a, const Scan& b) { return a.file == b.file; }), out.end());
    if (out.empty()) {
        error = product.label + ": the server lists no scans";
        return false;
    }
    return true;
}

bool UtilityMrms::frame(const Product& product, const Scan& scan, Frame& out, string& error) {
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        error = "GDAL not found - install the 'gdal' package";
        return false;
    }
    const auto step = (product.hi - product.validMin) / 254.0;
    out.validMin = product.validMin;
    out.step = step;
    out.utc = scan.utc;
    const auto cells = static_cast<qsizetype>(columns) * rows;
    const auto cachePath = QString::fromStdString(pathFor("mrmsd2_" + scan.file + ".u8z"));
    QFile cached{cachePath};
    if (cached.exists() && cached.open(QIODevice::ReadOnly)) {
        const auto packed = cached.readAll();
        cached.close();
        if (qUncompress(packed).size() == cells) {
            out.packed = packed;
            return true;
        }
    }
    const auto gzPath = QString::fromStdString(pathFor("mrmsg_" + scan.file));
    if (!QFile::exists(gzPath)) {
        const auto bytes = URL::getBytes(site + product.id + "/" + scan.file);
        QFile gz{gzPath};
        if (bytes.size() < 100 || !gz.open(QIODevice::WriteOnly) || gz.write(bytes) != bytes.size()) {
            error = "could not download " + scan.file;
            QFile::remove(gzPath);
            return false;
        }
    }
    const auto rawPath = QString::fromStdString(pathFor("mrmsr_" + scan.file + ".raw"));
    const auto hdrPath = rawPath.left(rawPath.size() - 4) + ".hdr";
    const auto cleanup = [&] {
        QFile::remove(rawPath);
        QFile::remove(hdrPath);
        QFile::remove(rawPath + ".aux.xml");
    };
    QProcess translate;
    translate.start(QString::fromStdString(binDir) + "/gdal_translate",
                    {"-q", "-of", "ENVI", "-ot", "Float32", "/vsigzip/" + gzPath, rawPath});
    translate.waitForFinished(180000);
    if (translate.exitStatus() != QProcess::NormalExit || translate.exitCode() != 0) {
        error = "gdal_translate failed on " + scan.file + ": " + translate.readAllStandardError().trimmed().toStdString();
        QFile::remove(gzPath);   // a bad download would fail again; fetch it afresh next time
        cleanup();
        return false;
    }
    // the picture is placed by this grid, so refuse anything that is not the CONUS grid it assumes
    QFile header{hdrPath};
    const auto headerText = header.open(QIODevice::ReadOnly) ? QString::fromUtf8(header.readAll()) : QString{};
    const auto number = [&headerText] (const QString& key) {
        const auto match = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(headerText);
        return match.hasMatch() ? match.captured(1).toDouble() : -1e9;
    };
    const auto mapInfo = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(headerText);
    const bool gridOk = static_cast<int>(number("samples")) == columns && static_cast<int>(number("lines")) == rows &&
        mapInfo.hasMatch() && std::abs(mapInfo.captured(1).toDouble() - west) < 0.01 &&
        std::abs(mapInfo.captured(2).toDouble() - north) < 0.01 && std::abs(mapInfo.captured(3).toDouble() - cell) < 1e-4;
    if (!gridOk) {
        error = scan.file + " is not on the expected 7000 x 3500 CONUS grid";
        cleanup();
        return false;
    }
    QFile raw{rawPath};
    if (!raw.open(QIODevice::ReadOnly) || raw.size() != cells * 4) {
        error = "unexpected decoded size for " + scan.file;
        cleanup();
        return false;
    }
    const auto floats = raw.readAll();
    raw.close();
    cleanup();
    const auto * values = reinterpret_cast<const float *>(floats.constData());
    QByteArray indices(cells, 0);
    auto * dst = reinterpret_cast<uchar *>(indices.data());
    for (qsizetype i = 0; i < cells; i += 1) {
        const double v = values[i];
        if (!std::isnan(v) && v >= product.validMin) {
            dst[i] = static_cast<uchar>(std::clamp(1 + static_cast<int>(std::lround((v - product.validMin) / step)), 1, 255));
        }
    }
    out.packed = qCompress(indices, 1);
    QFile cache{cachePath};
    if (cache.open(QIODevice::WriteOnly)) {
        cache.write(out.packed);
    }
    QFile::remove(gzPath);   // the decoded grid is what is kept
    return true;
}
