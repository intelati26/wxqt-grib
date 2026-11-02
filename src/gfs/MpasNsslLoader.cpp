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
#include "gfs/GdalRaster.h"
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

    QString indexPath(const std::string& tag, const GfsData::Run& run) {
        return cacheDir() + (tag == "mpasht2" ? QString{} : QString::fromStdString(tag) + "_") + QString::fromStdString(run.id()) + "_tar.txt";
    }

    // The tar's entries ("name offset size" a line) up to and including the one wanted: the headers are walked by range, a file at a time, and what was seen is kept for the next hour.
    bool findMember(const std::string& tag, const GfsData::Run& run, const std::string& name, Member& out, std::string& error) {
        std::map<std::string, Member> seen;
        long long next = 0;
        QFile file{indexPath(tag, run)};
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
        const auto url = MpasNsslLoader::tarUrl(run, tag);
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
            QFile keep{indexPath(tag, run)};
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
    QString hourFile(const std::string& tag, const GfsData::Run& run, int hour, std::string& error) {
        const auto path = cacheDir() + (tag == "mpasht2" ? QString{} : QString::fromStdString(tag) + "_") + QString::fromStdString(run.id()) + "_f" + QString::fromStdString(pad2(hour)) + ".grib2";
        std::lock_guard lock{lockFor(path)};
        if (QFile::exists(path)) {
            return path;
        }
        const auto name = tag + "_nssl_" + run.id() + "f" + pad2(hour) + ".grib2.gz";
        Member member;
        if (!findMember(tag, run, name, member, error)) {
            return {};
        }
        const auto packed = URL::getBytesRange(MpasNsslLoader::tarUrl(run, tag), member.offset, member.offset + member.size - 1);
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
        if (text.startsWith("entire atmosphere")) return "0-EATM";   // (also "... (considered as a single layer)")
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

    bool load(const std::string& tag, const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        const auto grib = hourFile(tag, run, hour, error);
        if (grib.isEmpty()) {
            return false;
        }
        const auto * found = bandsOf(grib, want.variable + " " + gdalLevel(want.level).toStdString(), error);
        if (found == nullptr) {
            return false;
        }
        const int band = found->front();   // (a field there more than once, an hour's maximum and a running total: the first, until a chart needs the other)
        const auto base = cacheDir() + "tmp_" + QString::number(reinterpret_cast<quintptr>(&want), 16) + "_" + QString::number(band) + "_" + QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
        GdalRaster::Options options;
        options.gdalBin = QString::fromStdString(UtilityGrib::gdalBinDir());
        options.warp = {true, west, south, east, north, step};
        options.band = band;
        options.timeoutMs = 360000;
        std::vector<float> values;
        int bands = 0;
        if (!GdalRaster::read(grib, base, options, out, bands, values, error) || bands != 1) {
            error = "gdal failed on " + want.variable + " " + want.level + (error.empty() ? "" : ": " + error);
            return false;
        }
        out.values = std::move(values);
        for (auto& v : out.values) {
            if (v < -9000.0f || v > 1e19f) {
                v = std::nanf("");
            }
        }
        return true;
    }
}

std::string MpasNsslLoader::tarUrl(const GfsData::Run& run, const std::string& tag) {
    return "https://data.nssl.noaa.gov/thredds/fileServer/FRDD/NSSL-MPAS/" + run.date.substr(0, 4) + "/" + run.date.substr(2) + run.cycle + "_" + tag + ".tar";
}

void MpasNsslLoader::registerLoader() {
    GfsData::setLoader("MPAS-HT", [] (const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) { return load("mpasht2", run, hour, want, out, error); });
    GfsData::setLoader("MPAS-RN", [] (const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) { return load("mpasrn", run, hour, want, out, error); });
}
