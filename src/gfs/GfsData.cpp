// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsData.h"
#include <cctype>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <future>
#include <mutex>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSaveFile>
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
    s.fileUrl = [] (const Run& run, int hour, const std::string&) {
        return "https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs." + run.date + "/" + run.cycle + "/atmos/gfs.t" + run.cycle + "z.pgrb2.0p25.f" + pad(hour, 3);
    };
    s.cycleHours = 6;
    s.lagHours = 3;
    s.probeHour = 0;
    return s;
}

// NCEP's AI global model (GraphCast-based, 0.25 degree, every 6 hours to 16 days): the pressure levels (height, temperature, humidity as specific humidity, vertical motion, wind) and the
// surface (10 m wind, 2 m temperature, sea level pressure, precipitation in 6 hour pieces) are separate files, in the NOAA GraphCast GFS open data bucket on AWS (the same layout as NOMADS, and it keeps the history)
GfsData::Source GfsData::aigfs() {
    Source s;
    s.id = "AIGFS";
    s.label = "NOAA/NCEP AIGFS 0.25 degree (an AI model)";
    s.fileUrl = [] (const Run& run, int hour, const std::string& file) {
        return "https://noaa-nws-graphcastgfs-pds.s3.amazonaws.com/aigfs." + run.date + "/" + run.cycle + "/model/atmos/grib2/aigfs.t" + run.cycle + "z." + (file.empty() ? "pres" : file) + ".f" + pad(hour, 3) +
            ".grib2";
    };
    s.fileOf = [] (const Want& want) { return want.level.size() > 3 && want.level.compare(want.level.size() - 3, 3, " mb") == 0 ? std::string{"pres"} : std::string{"sfc"}; };
    s.probeFile = "pres";
    s.cycleHours = 6;
    s.lagHours = 4;
    s.probeHour = 0;
    return s;
}

// The Global Ensemble Forecast System's mean and spread of its 30 members (0.5 degree to 16 days, every 3 hours to 240 and then 6). Each statistic is its own file ("avg-a", "spr-a"; the "-s"
// files are the 0.25 degree surface set, to hour 240, which holds what the 0.5 degree one does not: surface-based CAPE, gusts, dew point, helicity), in NOAA's open data bucket on AWS
GfsData::Source GfsData::gefs() {
    Source s;
    s.id = "GEFS";
    s.label = "NOAA/NCEP GEFS mean and spread, 0.5 degree";
    s.fileUrl = [] (const Run& run, int hour, const std::string& file) {
        const bool spread = file.compare(0, 3, "spr") == 0, surface = file.size() > 1 && file.back() == 's';
        const bool member = file.size() == 5 && (file[0] == 'c' || file[0] == 'p') && std::isdigit(static_cast<unsigned char>(file[1])) && std::isdigit(static_cast<unsigned char>(file[2]));   // "p05-a": member 5
        const std::string kind = member ? "ge" + file.substr(0, 3) : spread ? "gespr" : "geavg";
        return "https://noaa-gefs-pds.s3.amazonaws.com/gefs." + run.date + "/" + run.cycle + "/atmos/" + (surface ? "pgrb2sp25/" : "pgrb2ap5/") + kind + ".t" + run.cycle + "z." + (surface ? "pgrb2s.0p25" : "pgrb2a.0p50") +
            ".f" + pad(hour, 3);
    };
    s.fileOf = [] (const Want& want) {
        const auto& v = want.variable;
        const bool surfaceSet = v == "GUST" || v == "DPT" || v == "VIS" || v == "HLCY" || v == "MSLET" || (v == "CAPE" && want.level == "surface") || (v == "CIN" && want.level == "surface");
        const bool member = want.stat.size() == 3 && (want.stat[0] == 'c' || want.stat[0] == 'p') && std::isdigit(static_cast<unsigned char>(want.stat[1])) && std::isdigit(static_cast<unsigned char>(want.stat[2]));   // a member: "c00" the control, "p01" ... "p30"
        return (member ? want.stat : std::string{want.stat == "spr" ? "spr" : "avg"}) + (surfaceSet ? "-s" : "-a");
    };
    s.defaultDetail = "*";
    s.probeFile = "avg-a";
    s.cycleHours = 6;
    s.lagHours = 6;
    s.probeHour = 384;   // the files arrive over several hours: the run is there when its last one is
    return s;
}

// NCEP's hurricane model (HAFS version A and B), run for each active storm and invest on NOMADS: a storm-following grid of 0.02 degrees (the atmosphere and the simulated satellite as files
// of their own, 3 hourly to 126 hours) and one file of waves for all the hours (the hour is in the record, not the file name)
GfsData::Source GfsData::hafs(const std::string& model, const std::string& storm) {
    Source s;
    const std::string letter = model == "HAFSB" ? "b" : "a";
    s.id = model + "-" + storm;
    s.label = std::string{"NOAA/NCEP HAFS-"} + (letter == "b" ? "B" : "A") + " storm-following grid, 2 km";
    s.fileUrl = [letter, storm] (const Run& run, int hour, const std::string& file) {
        const auto base = "https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfs" + letter + "." + run.date + "/" + run.cycle + "/" + storm + "." + run.date + run.cycle + ".hfs" + letter + ".";
        return file == "swath" ? base + "parent.swath.grb2" : file == "trak" ? base + "trak.atcfunix" : file == "ww3" ? base + "ww3.grb2" : base + (file.empty() ? "storm.atm" : file) + ".f" + pad(hour, 3) + ".grb2";
    };
    s.fileOf = [] (const Want& want) {
        return want.stat == "swath" ? std::string{"swath"} : want.stat == "ww3" ? std::string{"ww3"} : want.variable.compare(0, 3, "var") == 0 ? std::string{"storm.sat"} : std::string{"storm.atm"};
    };
    s.defaultDetail = "*";
    s.extraMissing = 9999.0f;
    s.probeFile = "storm.atm";
    s.cycleHours = 6;
    s.lagHours = 4;
    s.probeHour = 126;   // a run is written hour by hour, and its track and waves only when it is done: it is there when its last hour is
    s.maxParallel = 4;   // NOMADS limits the number of requests at once
    s.cyclesToTry = 4;
    return s;
}

GfsData::Source GfsData::rrfs() {
    Source s;
    s.id = "RRFS";
    s.label = "NOAA/NCEP RRFS 3 km";
    s.fileUrl = [] (const Run& run, int hour, const std::string& file) {
        return "https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs." + run.date + "/" + run.cycle + "/rrfs.t" + run.cycle + "z." + (file.empty() ? "2dfld.3km" : file) + ".f" + pad(hour, 3) + ".conus.grib2";
    };
    s.fileOf = [] (const Want& want) {
        return want.level.size() > 3 && want.level.compare(want.level.size() - 3, 3, " mb") == 0 && want.level.find("above ground") == std::string::npos && want.level.find('-') == std::string::npos ? std::string{"prslev.3km"} : std::string{"2dfld.3km"};
    };
    s.probeFile = "2dfld.3km";
    s.cycleHours = 1;
    s.lagHours = 2;
    s.probeHour = 0;
    s.cyclesToTry = 8;
    s.warp = {true, 0.03, -127.0, 22.0, -65.0, 52.0};
    return s;
}

GfsData::Source GfsData::refs() {
    Source s;
    s.id = "REFS";
    s.label = "NOAA/NCEP REFS";
    s.fileUrl = [] (const Run& run, int hour, const std::string& file) {
        const std::string base = "https://noaa-rrfs-ops-pds.s3.amazonaws.com/";
        if (file.compare(0, 5, "rrfs/") == 0) {   // the deterministic RRFS run of the same cycle, for the charts that put it with the members
            return base + "rrfs." + run.date + "/" + run.cycle + "/rrfs.t" + run.cycle + "z." + file.substr(5) + ".f" + pad(hour, 3) + ".conus.grib2";
        }
        if (file.compare(0, 4, "ens:") == 0) {   // the ready-made products: the hour has two digits there
            return base + "refs." + run.date + "/" + run.cycle + "/ensprod/refs.t" + run.cycle + "z." + file.substr(4) + ".f" + pad(hour, 2) + ".conus.grib2";
        }
        const auto slash = file.find('/');   // "m003/2dfld": a member and its file
        const auto member = file.substr(0, slash);
        return base + "rrfsens." + run.date + "/" + run.cycle + "/" + member + "/rrfs.t" + run.cycle + "z." + member + "." + file.substr(slash + 1) + "nomads.3km.f" + pad(hour, 3) + ".conus.grib2";
    };
    s.fileOf = [] (const Want& want) {
        if (want.stat.compare(0, 4, "ens:") == 0) {
            return want.stat;
        }
        const bool levels = want.level.size() > 3 && want.level.compare(want.level.size() - 3, 3, " mb") == 0 && want.level.find("above ground") == std::string::npos && want.level.find('-') == std::string::npos;
        if (want.stat == "rrfs") {
            return std::string{"rrfs/"} + (levels ? "prslev.3km" : "2dfld.3km");
        }
        return (want.stat.empty() ? std::string{"m001"} : want.stat) + (levels ? "/prslev" : "/2dfld");
    };
    s.defaultDetail = "*";
    s.probeFile = "m001/2dfld";
    s.cycleHours = 6;
    s.lagHours = 5;
    s.probeHour = 60;   // a run is there when its last hour is
    s.cyclesToTry = 4;
    s.warp = {true, 0.03, -127.0, 22.0, -65.0, 52.0};
    return s;
}

GfsData::Source GfsData::gfsWave() {
    Source s;
    s.id = "GFS-WAVE";
    s.label = "NOAA/NCEP GFS-Wave 0.16 degree";
    s.fileUrl = [] (const Run& run, int hour, const std::string&) {
        return "https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs." + run.date + "/" + run.cycle + "/wave/gridded/gfswave.t" + run.cycle + "z.global.0p16.f" + pad(hour, 3) + ".grib2";
    };
    s.cycleHours = 6;
    s.lagHours = 5;
    s.probeHour = 384;   // a run is there when its last hour is
    s.extraMissing = 9999.0f;   // over land
    return s;
}

GfsData::Source GfsData::gefsWave() {
    Source s;
    s.id = "GEFS-WAVE";
    s.label = "NOAA/NCEP GEFS-Wave, control member, 0.25 degree";
    s.fileUrl = [] (const Run& run, int hour, const std::string&) {
        return "https://noaa-gefs-pds.s3.amazonaws.com/gefs." + run.date + "/" + run.cycle + "/wave/gridded/gefs.wave.t" + run.cycle + "z.c00.global.0p25.f" + pad(hour, 3) + ".grib2";
    };
    s.cycleHours = 6;
    s.lagHours = 7;
    s.probeHour = 384;
    s.extraMissing = 9999.0f;
    return s;
}

GfsData::Source GfsData::href() {
    Source s;
    s.id = "HREF";
    s.label = "NOAA/NCEP HREF, 10 member 3 km ensemble";
    s.fileUrl = [] (const Run& run, int hour, const std::string& file) {
        return "https://nomads.ncep.noaa.gov/pub/data/nccf/com/href/prod/href." + run.date + "/ensprod/href.t" + run.cycle + "z.conus." + (file.empty() ? "mean" : file) + ".f" + pad(hour, 2) + ".grib2";
    };
    s.fileOf = [] (const Want& want) { return want.stat.empty() ? std::string{"mean"} : want.stat; };
    s.defaultDetail = "*";
    s.probeFile = "mean";
    s.cycleHours = 6;
    s.lagHours = 4;
    s.probeHour = 36;   // the 06 and 18Z runs go to 36 hours, the others to 48
    s.cyclesToTry = 5;
    s.maxParallel = 4;   // NOMADS
    s.warp = {true, 0.03, -127.0, 22.0, -65.0, 52.0};
    return s;
}

GfsData::Source GfsData::nbm() {
    Source s;
    s.id = "NBM";
    s.label = "NOAA/NWS National Blend of Models v4, 2.5 km";
    s.fileUrl = [] (const Run& run, int hour, const std::string&) {
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
        const auto head = config.bytes(fileUrl(candidate, source.probeHour, source.probeFile) + ".idx", 0, 200);
        if (head.startsWith("1:0:d=")) {
            run = candidate;
            return true;
        }
    }
    return false;
}

bool GfsData::one(const Run& run, int hour, const std::string& file, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const {
    const auto * record = GfsGrid::find(index, want.variable, want.level, want.forecast, want.detail.empty() ? source.defaultDetail : want.detail);
    if (!record) {
        error = source.id + " has no " + want.variable + " " + want.level + (want.forecast.empty() ? "" : " (" + want.forecast + ")") + " in this run";
        return false;
    }
    // The name says what the field is: the model, the run, the hour, the file, the record, and how it was decoded (the grid it was warped to and which value means "no data"), so a change in
    // either is a different entry and an old one is never served in its place.
    const QString decoded = source.warp.enabled ? QString{"w%1_%2_%3_%4_%5"}.arg(source.warp.step).arg(source.warp.west).arg(source.warp.south).arg(source.warp.east).arg(source.warp.north) : QString{"p"};
    const QString missing = std::isnan(source.extraMissing) ? QString{} : QString{"_m%1"}.arg(static_cast<double>(source.extraMissing));
    QString name = QString::fromStdString(source.id + "_" + run.id() + "_f" + pad(hour, 3) + "_" + file + "_") + QString::fromStdString(want.variable + "_" + want.level + "_" + want.forecast + "_" + want.detail) + "_" + decoded + missing;
    name.replace(QRegularExpression{"[^A-Za-z0-9_.-]"}, "-");
    if (name.size() > 170) {   // a file name has a limit: the head tells which run and hour (the partial file is found by it), a hash the rest
        name = name.left(110) + "_" + QString::number(qHash(name), 16);
    }
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
    const auto url = fileUrl(run, hour, file);
    const auto slice = config.bytes(url, record->start, record->end);
    if (slice.size() < 100 || !slice.startsWith("GRIB")) {
        error = "could not download " + want.variable + " " + want.level;
        return false;
    }
    // the GRIB message stays with the decoded grid (the file made of what was downloaded for a run and hour is joined from these)
    const auto gribPath = config.cacheFolder + "/" + name + ".grb2";
    const auto rawPath = config.cacheFolder + "/" + name + ".raw";
    const auto hdrPath = config.cacheFolder + "/" + name + ".hdr";
    const auto cleanup = [&] {
        QFile::remove(rawPath);
        QFile::remove(hdrPath);
        QFile::remove(rawPath + ".aux.xml");
    };
    {
        QSaveFile f{gribPath};   // written whole or not at all: another screen may be reading the folder
        if (!f.open(QIODevice::WriteOnly) || f.write(slice) != slice.size() || !f.commit()) {
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
        if (std::abs(value) > 1e19f || value == source.extraMissing || (source.warp.enabled && value < -9998.5f)) {   // GRIB's missing value, and the warp's where the grid does not reach
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
    QSaveFile keep{cachePath};
    if (keep.open(QIODevice::WriteOnly)) {
        keep.write(store);
        keep.commit();
    }
    out = std::move(g);
    return true;
}

QString GfsData::partialGrib(const Run& run, int hour, const std::string& file) const {
    // the messages kept for this run, hour and file, joined (a GRIB file is only messages one after the other): made when asked, from whatever has been downloaded, in any session
    QString prefix = QString::fromStdString(source.id + "_" + run.id() + "_f" + pad(hour, 3) + "_" + file + "_");
    prefix.replace(QRegularExpression{"[^A-Za-z0-9_.-]"}, "-");
    const QDir dir{config.cacheFolder};
    const auto parts = dir.entryInfoList({prefix + "*.grb2"}, QDir::Files, QDir::Name);
    if (parts.isEmpty()) {
        return {};
    }
    const std::lock_guard lock{partialMutex};
    const auto path = config.cacheFolder + "/" + prefix + "joined.partial";   // the pruning removes these: they are made to be read once
    QSaveFile joined{path};
    if (!joined.open(QIODevice::WriteOnly)) {
        return {};
    }
    for (const auto& part : parts) {
        QFile in{part.absoluteFilePath()};
        if (in.open(QIODevice::ReadOnly)) {
            joined.write(in.readAll());
        }
    }
    return joined.commit() ? path : QString{};
}

bool GfsData::load(const Run& run, int hour, const std::vector<Want>& wants, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const {
    std::vector<Need> needs;
    for (const auto& w : wants) {
        needs.push_back({hour, w});
    }
    return load(run, needs, out, error);
}

namespace {
    // run job(0) ... job(count - 1) on at most `limit` threads (a fetch of every member at once would be dozens of connections and decoders together)
    void runLimited(size_t count, int limit, const std::function<void(size_t)>& job) {
        std::atomic<size_t> next{0};
        const size_t workers = std::min<size_t>(static_cast<size_t>(std::max(limit, 1)), count);
        std::vector<std::future<void>> running;
        for (size_t w = 0; w < workers; w++) {
            running.push_back(std::async(std::launch::async, [&] {
                for (size_t i = next++; i < count; i = next++) {
                    job(i);
                }
            }));
        }
        for (auto& r : running) {
            r.get();
        }
    }
}

bool GfsData::load(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const {
    // one index per forecast hour and file involved: fetched together (they do not depend on each other), a few at a time
    std::vector<std::pair<int, std::string>> keys;
    for (const auto& need : needs) {
        const auto key = std::make_pair(need.hour, fileFor(need.want));
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            keys.push_back(key);
        }
    }
    std::vector<std::vector<GfsGrid::IdxRecord>> parsed(keys.size());
    runLimited(keys.size(), source.maxParallel, [&] (size_t i) {
        const auto idxBytes = config.bytes(fileUrl(run, keys[i].first, keys[i].second) + ".idx", 0, -1);
        parsed[i] = GfsGrid::parseIdx(idxBytes.toStdString());
    });
    std::map<std::pair<int, std::string>, std::vector<GfsGrid::IdxRecord>> indexes;
    for (size_t i = 0; i < keys.size(); i++) {
        if (parsed[i].empty()) {
            error = "could not read the " + source.id + " index for " + run.id() + " f" + pad(keys[i].first, 3) + (keys[i].second.empty() ? "" : " (" + keys[i].second + ")");
            return false;
        }
        indexes[keys[i]] = std::move(parsed[i]);
    }
    struct Result {
        GfsGrid::Grid grid;
        std::string error;
        bool ok{false};
        bool absent{false};
    };
    std::vector<Result> results(needs.size());
    runLimited(needs.size(), source.maxParallel, [&] (size_t i) {   // the downloads and the decoding, a few at a time
        const auto& need = needs[i];
        Result& r = results[i];
        const auto file = fileFor(need.want);
        const auto& index = indexes.at(std::make_pair(need.hour, file));
        if (need.hour == 0 && !GfsGrid::find(index, need.want.variable, need.want.level, need.want.forecast, need.want.detail)) {
            r.ok = true;   // nothing accumulated yet at hour 0
            r.absent = true;
            return;
        }
        r.ok = one(run, need.hour, file, index, need.want, r.grid, r.error);
    });
    bool all = true;
    for (size_t i = 0; i < results.size(); i++) {
        auto& r = results[i];
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
