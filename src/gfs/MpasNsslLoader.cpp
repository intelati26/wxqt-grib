// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/MpasNsslLoader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include "gfs/GfsCache.h"
#include "gfs/GfsGrid.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "util/UtilityGzip.h"

namespace {
    // the grid the fields are warped onto: the HRRR's domain at about the model's own 3 km
    constexpr double west = -127.0, east = -65.0, south = 22.0, north = 52.0, step = 0.03;

    QString cacheDir() {
        const auto dir = GfsCache::folder() + "/mpas-nssl/";
        QDir{}.mkpath(dir);
        return dir;
    }

    std::string pad2(int hour) {
        char text[8];
        std::snprintf(text, sizeof text, "%02d", hour);
        return text;
    }

    struct Member {
        long long offset{0};   // of the file's bytes in the tar
        long long size{0};
    };

    // one download or decode of a run's hour at a time: the charts of a screen ask for many fields of the same hour at once
    std::mutex& lockFor(const QString& key) {
        static std::mutex guard;
        static std::map<QString, std::unique_ptr<std::mutex>> locks;
        const std::lock_guard lock{guard};
        auto& slot = locks[key];
        if (!slot) {
            slot = std::make_unique<std::mutex>();
        }
        return *slot;
    }

    QString indexPath(const GfsData::Run& run) {
        return cacheDir() + QString::fromStdString(run.id()) + "_tar.txt";
    }

    // The tar's entries ("name offset size" a line) up to and including the one wanted: the headers are walked by range, a file at a time, and what was seen is kept for the next hour.
    bool findMember(const GfsData::Run& run, const std::string& name, Member& out, std::string& error) {
        std::map<std::string, Member> seen;
        long long next = 0;
        QFile file{indexPath(run)};
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in{&file};
            while (!in.atEnd()) {
                const auto parts = in.readLine().split(' ');
                if (parts.size() == 4) {
                    seen[parts[0].toStdString()] = {parts[1].toLongLong(), parts[2].toLongLong()};
                    next = parts[3].toLongLong();
                }
            }
            file.close();
        }
        const auto found = [&] {
            const auto it = seen.find(name);
            if (it == seen.end()) {
                return false;
            }
            out = it->second;
            return true;
        };
        if (found()) {
            return true;
        }
        const auto url = MpasNsslLoader::tarUrl(run);
        QByteArray add;
        bool done = false;
        for (int guard = 0; guard < 400 && !done; guard++) {
            const auto header = URL::getBytesRange(url, next, next + 511);
            if (header.size() < 512 || header[0] == '\0') {
                break;
            }
            const std::string entry{header.constData(), strnlen(header.constData(), 100)};
            const long long size = QByteArray{header.constData() + 124, 12}.trimmed().toLongLong(nullptr, 8);
            const long long data = next + 512;
            next = data + (size + 511) / 512 * 512;
            if (size > 0) {
                const auto slash = entry.rfind('/');
                const auto base = slash == std::string::npos ? entry : entry.substr(slash + 1);
                seen[base] = {data, size};
                add += QByteArray::fromStdString(base) + " " + QByteArray::number(data) + " " + QByteArray::number(size) + " " + QByteArray::number(next) + "\n";
                done = base == name;
            } else {
                add += QByteArray::fromStdString(entry) + " 0 0 " + QByteArray::number(next) + "\n";
            }
        }
        if (!add.isEmpty()) {
            QFile keep{indexPath(run)};
            if (keep.open(QIODevice::Append)) {
                keep.write(add);
            }
        }
        if (!found()) {
            error = "the NSSL tar for the " + run.cycle + "Z run does not have " + name + " (yet)";
            return false;
        }
        return true;
    }

    // the unpacked GRIB2 file of an hour: fetched from the tar and gunzipped when it is not in the cache
    QString hourFile(const GfsData::Run& run, int hour, std::string& error) {
        const auto path = cacheDir() + QString::fromStdString(run.id()) + "_f" + QString::fromStdString(pad2(hour)) + ".grib2";
        std::lock_guard lock{lockFor(path)};
        if (QFile::exists(path)) {
            return path;
        }
        const auto name = "mpasht2_nssl_" + run.id() + "f" + pad2(hour) + ".grib2.gz";
        Member member;
        if (!findMember(run, name, member, error)) {
            return {};
        }
        const auto packed = URL::getBytesRange(MpasNsslLoader::tarUrl(run), member.offset, member.offset + member.size - 1);
        if (packed.size() != member.size) {
            error = "could not download " + name + " from NSSL";
            return {};
        }
        std::string plain;
        if (!UtilityGzip::gunzip(std::string{packed.constData(), static_cast<size_t>(packed.size())}, plain) || plain.compare(0, 4, "GRIB") != 0) {
            error = "could not unpack " + name;
            return {};
        }
        QSaveFile save{path};
        if (!save.open(QIODevice::WriteOnly) || save.write(plain.data(), static_cast<qint64>(plain.size())) != static_cast<qint64>(plain.size()) || !save.commit()) {
            error = "could not write " + path.toStdString();
            return {};
        }
        return path;
    }

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
        if (text == "entire atmosphere") return "0-EATM";
        if (text == "cloud base") return "0-CBL";
        if (text == "cloud top") return "0-CTL";
        return text;
    }

    // "ELEMENT LEVEL" -> the bands that have it, in the file's order, kept for each file
    const std::vector<int> * bandsOf(const QString& grib, const std::string& key, std::string& error) {
        static std::mutex guard;
        static std::map<QString, std::map<std::string, std::vector<int>>> maps;
        const std::lock_guard lock{guard};
        auto found = maps.find(grib);
        if (found == maps.end()) {
            QProcess info;
            info.start(QString::fromStdString(UtilityGrib::gdalBinDir()) + "/gdalinfo", {"-json", grib});
            info.waitForFinished(120000);
            std::map<std::string, std::vector<int>> map;
            const auto document = QJsonDocument::fromJson(info.readAllStandardOutput());
            for (const auto& b : document.object().value("bands").toArray()) {
                const auto metadata = b.toObject().value("metadata").toObject().value("").toObject();
                map[(metadata.value("GRIB_ELEMENT").toString() + " " + metadata.value("GRIB_SHORT_NAME").toString()).toStdString()].push_back(b.toObject().value("band").toInt());
            }
            found = maps.emplace(grib, std::move(map)).first;
        }
        const auto it = found->second.find(key);
        if (it == found->second.end()) {
            error = "NSSL's MPAS file has no " + key;
            return nullptr;
        }
        return &it->second;
    }

    bool load(const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        const auto grib = hourFile(run, hour, error);
        if (grib.isEmpty()) {
            return false;
        }
        const auto * bands = bandsOf(grib, want.variable + " " + gdalLevel(want.level).toStdString(), error);
        if (bands == nullptr) {
            return false;
        }
        const int band = bands->front();   // (a field there more than once, an hour's maximum and a running total: the first, until a chart needs the other)
        const auto base = cacheDir() + "tmp_" + QString::number(reinterpret_cast<quintptr>(&want), 16) + "_" + QString::number(band) + "_" + QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
        const auto raw = base + ".raw", hdr = base + ".hdr", tif = base + ".tif";
        const auto cleanup = [&] {
            QFile::remove(raw);
            QFile::remove(hdr);
            QFile::remove(tif);
            QFile::remove(raw + ".aux.xml");
        };
        const auto bin = QString::fromStdString(UtilityGrib::gdalBinDir());
        QProcess cut;   // the one band first (a 3 km Lambert grid, all of it): the warp then reads that and not the whole file
        cut.start(bin + "/gdal_translate", {"-q", "-b", QString::number(band), "-ot", "Float32", "-of", "GTiff", grib, tif});
        cut.waitForFinished(180000);
        QProcess warp;
        warp.start(bin + "/gdalwarp", {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-r", "bilinear", "-dstnodata", "-9999", "-of", "ENVI", "-ot", "Float32",
                                       "-te", QString::number(west), QString::number(south), QString::number(east), QString::number(north),
                                       "-tr", QString::number(step, 'f', 4), QString::number(step, 'f', 4), tif, raw});
        warp.waitForFinished(180000);
        if (warp.exitStatus() != QProcess::NormalExit || warp.exitCode() != 0) {
            error = "gdalwarp failed on " + want.variable + " " + want.level + ": " + warp.readAllStandardError().trimmed().toStdString();
            cleanup();
            return false;
        }
        QFile hdrFile{hdr}, rawFile{raw};
        const auto text = hdrFile.open(QIODevice::ReadOnly) ? QString::fromUtf8(hdrFile.readAll()) : QString{};
        const auto data = rawFile.open(QIODevice::ReadOnly) ? rawFile.readAll() : QByteArray{};
        cleanup();
        const auto number = [&text] (const QString& key) {
            const auto m = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(text);
            return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
        };
        const auto info = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(text);
        out.columns = static_cast<int>(number("samples"));
        out.rows = static_cast<int>(number("lines"));
        if (out.columns <= 0 || out.rows <= 0 || !info.hasMatch() || data.size() != static_cast<qsizetype>(out.columns) * out.rows * 4) {
            error = "the decoded MPAS field is not the size it says";
            return false;
        }
        out.step = info.captured(3).toDouble();
        out.lon0 = info.captured(1).toDouble() + out.step / 2.0;
        out.lat0 = info.captured(2).toDouble() - out.step / 2.0;
        out.values.resize(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows));
        std::memcpy(out.values.data(), data.constData(), static_cast<size_t>(data.size()));
        if (QRegularExpression{"byte order\\s*=\\s*1"}.match(text).hasMatch()) {
            for (auto& v : out.values) {
                unsigned char b[4];
                std::memcpy(b, &v, 4);
                std::swap(b[0], b[3]);
                std::swap(b[1], b[2]);
                std::memcpy(&v, b, 4);
            }
        }
        for (auto& v : out.values) {
            if (v < -9000.0f || v > 1e19f) {
                v = std::nanf("");
            }
        }
        return true;
    }
}

std::string MpasNsslLoader::tarUrl(const GfsData::Run& run) {
    return "https://data.nssl.noaa.gov/thredds/fileServer/FRDD/NSSL-MPAS/" + run.date.substr(0, 4) + "/" + run.date.substr(2) + run.cycle + "_mpasht2.tar";
}

void MpasNsslLoader::registerLoader() {
    GfsData::setLoader("MPAS-HT", load);
}
