// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GdalRaster.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

void GdalRaster::removeScratch(const QString& base) {
    QFile::remove(base + ".raw");
    QFile::remove(base + ".hdr");
    QFile::remove(base + ".raw.aux.xml");
}

bool GdalRaster::translate(const QString& input, const QString& base, const Options& options, std::string& message) {
    QStringList args;
    QString program = "/gdal_translate";
    if (options.warp.enabled) {
        program = "/gdalwarp";
        args << "-q" << "-overwrite" << "-t_srs" << "EPSG:4326" << "-r" << "bilinear" << "-dstnodata" << "-9999" << "-of" << "ENVI" << "-ot" << "Float32"
             << "-te" << QString::number(options.warp.west, 'f', 3) << QString::number(options.warp.south, 'f', 3) << QString::number(options.warp.east, 'f', 3) << QString::number(options.warp.north, 'f', 3)
             << "-tr" << QString::number(options.warp.step, 'f', 4) << QString::number(options.warp.step, 'f', 4);
        if (options.band > 0) {
            args << "-srcband" << QString::number(options.band);
        }
    } else {
        args << "-q" << "-of" << "ENVI" << "-ot" << "Float32";
        if (options.band > 0) {
            args << "-b" << QString::number(options.band);
        }
        for (const int band : options.bandList) {
            args << "-b" << QString::number(band);
        }
    }
    args << options.extra << input << base + ".raw";
    QProcess process;
    process.start(options.gdalBin + program, args);
    // A run that outlives the timeout is still going (and exitStatus() says "normal, 0" of a process that has not exited): stop it and report it, or the half-written files are read as the result
    if (!process.waitForStarted(10000) || !process.waitForFinished(options.timeoutMs)) {
        const bool started = process.state() != QProcess::NotRunning;
        process.kill();
        process.waitForFinished(3000);
        message = started ? "gdal did not finish in time" : "gdal could not be started: " + process.errorString().toStdString();
        return false;
    }
    // GDAL prints errors about drivers a machine lacks: only the exit code counts
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        message = process.readAllStandardError().trimmed().toStdString();
        return false;
    }
    return true;
}

bool GdalRaster::readEnvi(const QString& rawPath, const QString& hdrPath, GfsGrid::Grid& g, int& bands, std::vector<float>& values, std::string& error, bool requirePlain) {
    QFile hdr{hdrPath};
    if (!hdr.open(QIODevice::ReadOnly)) {
        error = "gdal wrote no header for the grid";
        return false;
    }
    const auto hdrText = QString::fromUtf8(hdr.readAll());
    const auto number = [&hdrText] (const QString& key) {
        const auto m = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(hdrText);
        return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
    };
    const auto info = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(hdrText);
    g.columns = static_cast<int>(number("samples"));
    g.rows = static_cast<int>(number("lines"));
    bands = std::max(1, static_cast<int>(number("bands")));
    const bool plain = g.columns > 0 && g.rows > 0 && info.hasMatch() && info.captured(3).toDouble() > 0.0 && (!requirePlain || std::abs(info.captured(3).toDouble() - info.captured(4).toDouble()) < 1e-6);
    if (!plain && (requirePlain || g.columns <= 0 || g.rows <= 0)) {
        error = "the grid is not a plain latitude / longitude grid";
        return false;
    }
    if (info.hasMatch()) {
        g.step = info.captured(3).toDouble();
        g.lon0 = info.captured(1).toDouble() + g.step / 2.0;   // the header gives the corner of the first cell
        g.lat0 = info.captured(2).toDouble() - g.step / 2.0;
    }
    QFile raw{rawPath};
    const QByteArray bytes = raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
    const auto cells = static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows);
    if (static_cast<size_t>(bytes.size()) != cells * static_cast<size_t>(bands) * 4) {
        error = "the decoded field has the wrong size";
        return false;
    }
    // ENVI float data is stored in the machine's byte order unless the header says "byte order = 1"
    const bool bigEndian = QRegularExpression{"byte order\\s*=\\s*1"}.match(hdrText).hasMatch();
    values.resize(cells * static_cast<size_t>(bands));
    std::memcpy(values.data(), bytes.constData(), static_cast<size_t>(bytes.size()));
    if (bigEndian) {
        for (auto& value : values) {
            unsigned char b[4];
            std::memcpy(b, &value, 4);
            std::swap(b[0], b[3]);
            std::swap(b[1], b[2]);
            std::memcpy(&value, b, 4);
        }
    }
    return true;
}

bool GdalRaster::read(const QString& input, const QString& base, const Options& options, GfsGrid::Grid& g, int& bands, std::vector<float>& values, std::string& error) {
    std::string message;
    if (!translate(input, base, options, message)) {
        error = "gdal failed on " + input.toStdString() + (message.empty() ? "" : ": " + message);
        removeScratch(base);
        return false;
    }
    const bool ok = readEnvi(base + ".raw", base + ".hdr", g, bands, values, error, options.requirePlain);
    removeScratch(base);
    return ok;
}

namespace {
    // GDAL's name for a level ("2-HTGL", "50000-ISBL", "5000-2000-HTGL") from the NOAA way of writing it ("2 m above ground", "500 mb", "2000-5000 m above ground"); a name GDAL already uses is kept
    QString gdalLevel(const std::string& level) {
        const auto text = QString::fromStdString(level);
        static const QRegularExpression pressure{"^(\\d+) mb$"}, height{"^(\\d+) m above ground$"}, layer{"^(\\d+)-(\\d+) m above ground$"}, spd{"^(\\d+)-(\\d+) mb above ground$"};
        if (const auto m = pressure.match(text); m.hasMatch()) {
            return QString::number(m.captured(1).toInt() * 100) + "-ISBL";
        }
        if (const auto m = height.match(text); m.hasMatch()) {
            return m.captured(1) + "-HTGL";
        }
        if (const auto m = layer.match(text); m.hasMatch()) {
            return QString::number(std::max(m.captured(1).toInt(), m.captured(2).toInt())) + "-" + QString::number(std::min(m.captured(1).toInt(), m.captured(2).toInt())) + "-HTGL";
        }
        if (const auto m = spd.match(text); m.hasMatch()) {
            return QString::number(std::max(m.captured(1).toInt(), m.captured(2).toInt()) * 100) + "-" + QString::number(std::min(m.captured(1).toInt(), m.captured(2).toInt()) * 100) + "-SPDL";
        }
        if (text == "surface") return "0-SFC";
        if (text == "mean sea level") return "0-MSL";
        if (text.startsWith("entire atmosphere")) return "0-EATM";   // (also "... (considered as a single layer)")
        if (text == "cloud base") return "0-CBL";
        if (text == "cloud top") return "0-CTL";
        return text;
    }
}

int GdalRaster::bandOf(const QString& gdalBin, const QString& grib, const std::string& variable, const std::string& level, std::string& error) {
    static std::mutex guard;
    static std::map<QString, std::map<std::string, std::vector<int>>> maps;   // "ELEMENT LEVEL" -> the bands that have it, in the file's order, for each file
    const std::lock_guard lock{guard};
    auto found = maps.find(grib);
    if (found == maps.end()) {
        QProcess info;
        info.start(gdalBin + "/gdalinfo", {"-json", grib});
        info.waitForFinished(120000);
        std::map<std::string, std::vector<int>> map;
        const auto document = QJsonDocument::fromJson(info.readAllStandardOutput());
        for (const auto& b : document.object().value("bands").toArray()) {
            const auto metadata = b.toObject().value("metadata").toObject().value("").toObject();
            map[(metadata.value("GRIB_ELEMENT").toString() + " " + metadata.value("GRIB_SHORT_NAME").toString()).toStdString()].push_back(b.toObject().value("band").toInt());
        }
        found = maps.emplace(grib, std::move(map)).first;
    }
    const auto key = variable + " " + gdalLevel(level).toStdString();
    const auto it = found->second.find(key);
    if (it == found->second.end()) {
        error = "the file has no " + key;
        return 0;
    }
    return it->second.front();
}
