// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsData.h"
#include <cmath>
#include <cstring>
#include <future>
#include <mutex>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>

namespace {
    std::string pad(int value, int width) {
        auto text = std::to_string(value);
        return std::string(static_cast<size_t>(std::max<int>(0, width - static_cast<int>(text.size()))), '0') + text;
    }
}

GfsData::Source GfsData::gfs() {
    Source s;
    s.id = "GFS";
    s.label = "NOAA/NCEP GFS 0.25 degree";
    s.fileUrl = [] (const Run& run, int hour) {
        return "https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs." + run.date + "/" + run.cycle + "/atmos/gfs.t" + run.cycle + "z.pgrb2.0p25.f" + pad(hour, 3);
    };
    s.cycleHours = 6;
    s.lagHours = 3;
    s.probeHour = 0;
    return s;
}

GfsData::Source GfsData::nbm() {
    Source s;
    s.id = "NBM";
    s.label = "NOAA/NWS National Blend of Models v4, 2.5 km";
    s.fileUrl = [] (const Run& run, int hour) {
        return "https://noaa-nbm-grib2-pds.s3.amazonaws.com/blend." + run.date + "/" + run.cycle + "/core/blend.t" + run.cycle + "z.core.f" + pad(hour, 3) + ".co.grib2";
    };
    s.cycleHours = 1;      // a run every hour
    s.lagHours = 1;
    s.probeHour = 1;       // there is no hour 0 file
    s.cyclesToTry = 8;
    s.warp.enabled = true; // a Lambert conformal grid
    s.warp.step = 0.025;   // about the 2.5 km of the blend
    return s;
}

std::vector<int> GfsData::forecastHours() {
    std::vector<int> hours;
    for (int h = 0; h <= 240; h += 3) {
        hours.push_back(h);
    }
    for (int h = 246; h <= 384; h += 6) {
        hours.push_back(h);
    }
    return hours;
}

bool GfsData::latestRun(Run& run) const {
    const auto now = QDateTime::currentDateTimeUtc();
    // a run appears some time after its own time: start from the last cycle mark that long ago and go back
    auto start = now.addSecs(-static_cast<qint64>(source.lagHours) * 3600);
    start.setTime(QTime{start.time().hour() / source.cycleHours * source.cycleHours, 0});
    for (int back = 0; back < source.cyclesToTry; back++) {
        const auto t = start.addSecs(-static_cast<qint64>(back) * source.cycleHours * 3600);
        Run candidate{t.toString("yyyyMMdd").toStdString(), pad(t.time().hour(), 2)};
        const auto head = config.bytes(fileUrl(candidate, source.probeHour) + ".idx", 0, 200);
        if (head.startsWith("1:0:d=")) {
            run = candidate;
            return true;
        }
    }
    return false;
}

bool GfsData::one(const Run& run, int hour, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const {
    const auto * record = GfsGrid::find(index, want.variable, want.level, want.forecast, want.detail);
    if (!record) {
        error = source.id + " has no " + want.variable + " " + want.level + (want.forecast.empty() ? "" : " (" + want.forecast + ")") + " in this run";
        return false;
    }
    QString name = QString::fromStdString(source.id + "_" + run.id() + "_f" + pad(hour, 3) + "_" + want.variable + "_" + want.level + "_" + want.forecast + "_" + want.detail);
    name.replace(QRegularExpression{"[^A-Za-z0-9_.-]"}, "-");
    QDir{}.mkpath(config.cacheFolder);
    const auto cachePath = config.cacheFolder + "/" + name + ".gz4";
    constexpr int header = 4 + 4 + 8 + 8 + 8;
    QFile cached{cachePath};
    if (cached.open(QIODevice::ReadOnly)) {
        const auto all = cached.readAll();
        cached.close();
        if (all.size() > header) {
            GfsGrid::Grid g;
            std::memcpy(&g.columns, all.constData(), 4);
            std::memcpy(&g.rows, all.constData() + 4, 4);
            std::memcpy(&g.lon0, all.constData() + 8, 8);
            std::memcpy(&g.lat0, all.constData() + 16, 8);
            std::memcpy(&g.step, all.constData() + 24, 8);
            const auto floats = qUncompress(all.mid(header));
            if (g.columns > 0 && g.rows > 0 && floats.size() == static_cast<qsizetype>(g.columns) * g.rows * 4) {
                g.values.resize(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows));
                std::memcpy(g.values.data(), floats.constData(), static_cast<size_t>(floats.size()));
                out = std::move(g);
                return true;
            }
        }
    }
    const auto url = fileUrl(run, hour);
    const auto slice = config.bytes(url, record->start, record->end);
    if (slice.size() < 100 || !slice.startsWith("GRIB")) {
        error = "could not download " + want.variable + " " + want.level;
        return false;
    }
    {   // keep the message in the run's partial file as well
        const std::lock_guard lock{partialMutex};
        QFile partial{partialGrib(run, hour)};
        if (partial.open(QIODevice::WriteOnly | QIODevice::Append)) {
            partial.write(slice);
        }
    }
    const auto gribPath = config.cacheFolder + "/" + name + ".grib2";
    const auto rawPath = config.cacheFolder + "/" + name + ".raw";
    const auto hdrPath = config.cacheFolder + "/" + name + ".hdr";
    const auto cleanup = [&] {
        QFile::remove(gribPath);
        QFile::remove(rawPath);
        QFile::remove(hdrPath);
        QFile::remove(rawPath + ".aux.xml");
    };
    {
        QFile f{gribPath};
        if (!f.open(QIODevice::WriteOnly) || f.write(slice) != slice.size()) {
            error = "could not write " + gribPath.toStdString();
            cleanup();
            return false;
        }
    }
    QProcess translate;
    if (source.warp.enabled) {   // a grid that is not latitude / longitude: warped to one (bilinear: smooth, with no overshoot at the edge of a rain area)
        translate.start(QString::fromStdString(config.gdalBin) + "/gdalwarp",
                        {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-r", "bilinear", "-dstnodata", "-9999", "-of", "ENVI", "-ot", "Float32",
                         "-te", QString::number(source.warp.west, 'f', 3), QString::number(source.warp.south, 'f', 3), QString::number(source.warp.east, 'f', 3), QString::number(source.warp.north, 'f', 3),
                         "-tr", QString::number(source.warp.step, 'f', 4), QString::number(source.warp.step, 'f', 4), gribPath, rawPath});
    } else {
        translate.start(QString::fromStdString(config.gdalBin) + "/gdal_translate", {"-q", "-of", "ENVI", "-ot", "Float32", gribPath, rawPath});
    }
    translate.waitForFinished(180000);
    if (translate.exitStatus() != QProcess::NormalExit || translate.exitCode() != 0) {
        error = "gdal_translate failed on " + want.variable + " " + want.level + ": " + translate.readAllStandardError().trimmed().toStdString();
        cleanup();
        return false;
    }
    QFile hdr{hdrPath};
    const auto hdrText = hdr.open(QIODevice::ReadOnly) ? QString::fromUtf8(hdr.readAll()) : QString{};
    const auto number = [&hdrText] (const QString& key) {
        const auto m = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(hdrText);
        return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
    };
    const auto info = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(hdrText);
    GfsGrid::Grid g;
    g.columns = static_cast<int>(number("samples"));
    g.rows = static_cast<int>(number("lines"));
    const bool plain = g.columns > 0 && g.rows > 0 && info.hasMatch() && info.captured(3).toDouble() > 0.0 && std::abs(info.captured(3).toDouble() - info.captured(4).toDouble()) < 1e-6;
    if (!plain) {
        error = "the GFS grid is not a plain latitude / longitude grid";
        cleanup();
        return false;
    }
    g.step = info.captured(3).toDouble();
    g.lon0 = info.captured(1).toDouble() + g.step / 2.0;   // the header gives the corner of the first cell
    g.lat0 = info.captured(2).toDouble() - g.step / 2.0;
    QFile raw{rawPath};
    QByteArray bytes = raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
    cleanup();
    if (bytes.size() != static_cast<qsizetype>(g.columns) * g.rows * 4) {
        error = "the decoded GFS field has the wrong size";
        return false;
    }
    // ENVI float data is stored in the machine's byte order unless the header says "byte order = 1"
    const bool bigEndian = QRegularExpression{"byte order\\s*=\\s*1"}.match(hdrText).hasMatch();
    g.values.resize(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows));
    std::memcpy(g.values.data(), bytes.constData(), static_cast<size_t>(bytes.size()));
    if (bigEndian) {
        for (auto& value : g.values) {
            unsigned char b[4];
            std::memcpy(b, &value, 4);
            std::swap(b[0], b[3]);
            std::swap(b[1], b[2]);
            std::memcpy(&value, b, 4);
        }
    }
    for (auto& value : g.values) {
        if (std::abs(value) > 1e19f || (source.warp.enabled && value < -9998.5f)) {   // GRIB's missing value, and the warp's where the grid does not reach
            value = std::nanf("");
        }
    }
    QByteArray store(header, '\0');
    std::memcpy(store.data(), &g.columns, 4);
    std::memcpy(store.data() + 4, &g.rows, 4);
    std::memcpy(store.data() + 8, &g.lon0, 8);
    std::memcpy(store.data() + 16, &g.lat0, 8);
    std::memcpy(store.data() + 24, &g.step, 8);
    store += qCompress(QByteArray::fromRawData(reinterpret_cast<const char *>(g.values.data()), static_cast<qsizetype>(g.values.size() * 4)), 6);
    QFile keep{cachePath};
    if (keep.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        keep.write(store);
    }
    out = std::move(g);
    return true;
}

QString GfsData::partialGrib(const Run& run, int hour) const {
    return config.cacheFolder + "/gfs." + QString::fromStdString(run.id()) + ".f" + QString::fromStdString(pad(hour, 3)) + ".partial.grib2";
}

bool GfsData::load(const Run& run, int hour, const std::vector<Want>& wants, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const {
    std::vector<Need> needs;
    for (const auto& w : wants) {
        needs.push_back({hour, w});
    }
    return load(run, needs, out, error);
}

bool GfsData::load(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const {
    // one index per forecast hour involved
    std::map<int, std::vector<GfsGrid::IdxRecord>> indexes;
    for (const auto& need : needs) {
        if (indexes.find(need.hour) == indexes.end()) {
            const auto idxBytes = config.bytes(fileUrl(run, need.hour) + ".idx", 0, -1);
            indexes[need.hour] = GfsGrid::parseIdx(idxBytes.toStdString());
            if (indexes[need.hour].empty()) {
                error = "could not read the GFS index for " + run.id() + " f" + pad(need.hour, 3);
                return false;
            }
        }
    }
    struct Result {
        GfsGrid::Grid grid;
        std::string error;
        bool ok{false};
        bool absent{false};
    };
    std::vector<std::future<Result>> jobs;
    for (const auto& need : needs) {
        jobs.push_back(std::async(std::launch::async, [&, need] {
            Result r;
            const auto& index = indexes.at(need.hour);
            if (need.hour == 0 && !GfsGrid::find(index, need.want.variable, need.want.level, need.want.forecast, need.want.detail)) {
                r.ok = true;   // nothing accumulated yet at hour 0
                r.absent = true;
                return r;
            }
            r.ok = one(run, need.hour, index, need.want, r.grid, r.error);
            return r;
        }));
    }
    bool all = true;
    for (size_t i = 0; i < jobs.size(); i++) {
        auto r = jobs[i].get();
        if (r.ok) {
            if (!r.absent) {
                out[needs[i].want.key] = std::move(r.grid);
            }
        } else if (all) {
            error = r.error;
            all = false;
        }
    }
    return all;
}
