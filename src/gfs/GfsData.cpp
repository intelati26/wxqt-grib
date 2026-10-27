// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsData.h"
#include "util/NetPriority.h"
#include <cctype>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <future>
#include <mutex>
#include <set>
#include <tuple>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QRegularExpression>

namespace {
    std::string pad(int value, int width) {
        auto text = std::to_string(value);
        return std::string(static_cast<size_t>(std::max<int>(0, width - static_cast<int>(text.size()))), '0') + text;
    }
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
        if (source.wholeFile) {   // no index: the first bytes of a file say the run is there
            bool there = false;
            for (const auto& url : urlsFor(candidate, source.probeHour, source.probeFile)) {
                const auto resolved = resolveUrl(url);
                if (resolved.empty()) {
                    continue;
                }
                const auto head = config.bytes(resolved, 0, 3);
                if (head.startsWith("GRIB") || head.startsWith("CDF") || head.startsWith("\x89HDF") || head.startsWith("{")) {
                    there = true;
                    break;
                }
            }
            if (there) {
                run = candidate;
                return true;
            }
            continue;
        }
        const auto head = config.bytes(indexUrl(candidate, source.probeHour, source.probeFile), 0, 200);
        if (source.parseIndex ? head.startsWith("{") : head.startsWith("1:0:d=")) {
            run = candidate;
            return true;
        }
    }
    return false;
}

namespace {
    std::mutex loaderMutex;
    std::map<std::string, GfsData::Loader>& loaders() {
        static std::map<std::string, GfsData::Loader> list;
        return list;
    }
}

namespace {
    // The ENVI file gdal wrote (the header and the raw floats) as the geometry of a plain latitude / longitude grid and the values of every band, one band after the other. false with the reason
    // when the grid is not that or the file is not the size it says.
    bool readEnvi(const QString& rawPath, const QString& hdrPath, GfsGrid::Grid& g, int& bands, std::vector<float>& values, std::string& error) {
        QFile hdr{hdrPath};
        const auto hdrText = hdr.open(QIODevice::ReadOnly) ? QString::fromUtf8(hdr.readAll()) : QString{};
        const auto number = [&hdrText] (const QString& key) {
            const auto m = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(hdrText);
            return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
        };
        const auto info = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(hdrText);
        g.columns = static_cast<int>(number("samples"));
        g.rows = static_cast<int>(number("lines"));
        bands = std::max(1, static_cast<int>(number("bands")));
        const bool plain = g.columns > 0 && g.rows > 0 && info.hasMatch() && info.captured(3).toDouble() > 0.0 && std::abs(info.captured(3).toDouble() - info.captured(4).toDouble()) < 1e-6;
        if (!plain) {
            error = "the GFS grid is not a plain latitude / longitude grid";
            return false;
        }
        g.step = info.captured(3).toDouble();
        g.lon0 = info.captured(1).toDouble() + g.step / 2.0;   // the header gives the corner of the first cell
        g.lat0 = info.captured(2).toDouble() - g.step / 2.0;
        QFile raw{rawPath};
        const QByteArray bytes = raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
        const auto cells = static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows);
        if (static_cast<size_t>(bytes.size()) != cells * static_cast<size_t>(bands) * 4) {
            error = "the decoded GFS field has the wrong size";
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
}

bool GfsData::one(const Run& run, int hour, const std::string& file, const std::vector<GfsGrid::IdxRecord>& index, const Want& want, GfsGrid::Grid& out, std::string& error) const {
    const auto * record = GfsGrid::find(index, want.variable, want.level, want.forecast, want.detail.empty() ? source.defaultDetail : want.detail, source.matchRunTime ? run.id() : std::string{});
    if (!record) {
        error = source.id + " has no " + want.variable + " " + want.level + (want.forecast.empty() ? "" : " (" + want.forecast + ")") + " in this run";
        return false;
    }
    // The name says what the field is: the model, the run, the hour, the file, the record, and how it was decoded (the grid it was warped to and which value means "no data"), so a change in
    // either is a different entry and an old one is never served in its place.
    const QString decoded = source.warp.enabled ? QString{"w%1_%2_%3_%4_%5"}.arg(source.warp.step).arg(source.warp.west).arg(source.warp.south).arg(source.warp.east).arg(source.warp.north) : QString{"p"};
    const QString missing = std::isnan(source.extraMissing) ? QString{} : QString{"_m%1"}.arg(static_cast<double>(source.extraMissing));
    QString name = QString::fromStdString(source.id + "_" + run.id() + "_f" + pad(hour, 3) + "_" + file + "_") + QString::fromStdString(want.variable + "_" + want.level + "_" + want.forecast + "_" + want.detail) + "_" + decoded + missing;
    if (!want.stat.empty()) {   // an ensemble member of the same file and record is a field of its own
        name += "_" + QString::fromStdString(want.stat);
    }
    name.replace(QRegularExpression{"[^A-Za-z0-9_.-]"}, "-");
    if (name.size() > 170) {   // a file name has a limit: the head tells which run and hour (the partial file is found by it), a hash the rest
        name = name.left(110) + "_" + QString::number(qHash(name), 16);
    }
    QDir{}.mkpath(config.cacheFolder);
    const auto cachePath = config.cacheFolder + "/" + name + ".gz4";
    constexpr int header = 4 + 4 + 8 + 8 + 8 + 8 + 8;   // columns, rows, lon0, lat0, step, then the record's first and last byte in the file
    QFile cached{cachePath};
    if (cached.open(QIODevice::ReadOnly)) {
        const auto all = cached.readAll();
        cached.close();
        long long storedStart = -2, storedEnd = -2;
        if (all.size() > header) {
            std::memcpy(&storedStart, all.constData() + 32, 8);
            std::memcpy(&storedEnd, all.constData() + 40, 8);
        }
        // The index says where the record is in the file now: the same place and size as when it was kept is the cheap proof that the file was not posted again (a run's files do not
        // change once published; a replaced one moves or resizes its records). Anything else is fetched again and the entry rewritten.
        if (all.size() > header && storedStart == record->start && storedEnd == record->end) {
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
    // the decoded copy is private to this call: two screens (or two tiles) decoding the same field at once must not share it
    const auto scratch = name + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16) + "_" + QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
    const auto rawPath = config.cacheFolder + "/" + scratch + ".raw";
    const auto hdrPath = config.cacheFolder + "/" + scratch + ".hdr";
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
        const auto message = translate.readAllStandardError().trimmed().toStdString();
        error = "gdal_translate failed on " + want.variable + " " + want.level + ": " + message;
        if (message.find("libaec") != std::string::npos) {   // ECMWF's CCSDS compression: only a GDAL built with libaec reads it
            error = "this GDAL cannot read CCSDS (AEC) compressed GRIB, which ECMWF's IFS and AIFS use: it was built without libaec (" + want.variable + " " + want.level + ")";
        }
        cleanup();
        return false;
    }
    GfsGrid::Grid g;
    int bands = 0;
    std::vector<float> all;
    const bool read = readEnvi(rawPath, hdrPath, g, bands, all, error);
    cleanup();
    if (!read) {
        return false;
    }
    const size_t cells = static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows);
    const size_t skip = static_cast<size_t>(std::max(1, record->band) - 1) * cells;   // a message of several fields (U and V together): the band of this one
    if (all.size() < skip + cells) {
        error = "the message of " + want.variable + " " + want.level + " has no field " + std::to_string(record->band);
        cleanup();
        return false;
    }
    g.values.assign(all.begin() + static_cast<std::ptrdiff_t>(skip), all.begin() + static_cast<std::ptrdiff_t>(skip + cells));
    const double factor = source.valueScale ? source.valueScale(want) : 1.0;
    const double shift = source.valueOffset ? source.valueOffset(want) : 0.0;
    for (auto& value : g.values) {
        const bool missing = std::abs(value) > 1e19f || value == source.extraMissing || (source.warp.enabled && value < -9998.5f);
        if (!missing && (factor != 1.0 || shift != 0.0)) {
            value = static_cast<float>(value * factor + shift);
        }
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
    std::memcpy(store.data() + 32, &record->start, 8);
    std::memcpy(store.data() + 40, &record->end, 8);
    store += qCompress(QByteArray::fromRawData(reinterpret_cast<const char *>(g.values.data()), static_cast<qsizetype>(g.values.size() * 4)), 6);
    QSaveFile keep{cachePath};
    if (keep.open(QIODevice::WriteOnly)) {
        keep.write(store);
        keep.commit();
    }
    out = std::move(g);
    return true;
}

std::vector<std::string> GfsData::urlsFor(const Run& run, int hour, const std::string& file) const {
    if (source.candidates) {
        return source.candidates(run, hour, file);
    }
    return {fileUrl(run, hour, file)};
}

// a listing of a directory is kept for two minutes: the fields of one chart are all in the same one
std::string GfsData::resolveUrl(const std::string& url) const {
    const auto slash = url.rfind('/');
    if (slash == std::string::npos || url.find_first_of("?*", slash) == std::string::npos || url.find('=', slash) != std::string::npos) {   // (a "?" before a "=" is a query string, not a wildcard)
        return url;
    }
    static std::mutex mutex;
    static std::map<std::string, std::pair<std::chrono::steady_clock::time_point, std::string>> listings;
    const auto directory = url.substr(0, slash + 1);
    std::string html;
    {
        const std::lock_guard lock{mutex};
        const auto found = listings.find(directory);
        if (found != listings.end() && std::chrono::steady_clock::now() - found->second.first < std::chrono::minutes(2)) {
            html = found->second.second;
        }
    }
    if (html.empty()) {
        html = config.bytes(directory, 0, -1).toStdString();
        if (!html.empty()) {
            const std::lock_guard lock{mutex};
            listings[directory] = {std::chrono::steady_clock::now(), html};
        }
    }
    QString pattern = QRegularExpression::escape(QString::fromStdString(url.substr(slash + 1)));
    pattern.replace("\\?", "([^/\"])").replace("\\*", "([^/\"]*)");
    const QRegularExpression re{"href=\"(" + pattern + ")\""};
    // of several matches the one whose '?' and '*' hold the longest, then the largest, text (the rest of the convective day is "24hr" or "9hr", and the 4 hour period is always there too)
    std::string best;
    QString bestKey;
    auto matches = re.globalMatch(QString::fromStdString(html));
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString key;
        for (int i = 2; i <= match.lastCapturedIndex(); i++) {
            key += match.captured(i);
        }
        const auto rank = QString::number(key.size()).rightJustified(3, '0') + key;
        if (best.empty() || rank > bestKey) {
            best = match.captured(1).toStdString();
            bestKey = rank;
        }
    }
    return best.empty() ? std::string{} : directory + best;
}

// A field of a source with no index (GEPS, SPC's post-processing): the file is kept whole for the run and hour (a NetCDF one is read by range), and the field is the mean or spread of its
// messages, or one of them (a member, a variable of a file of several).
bool GfsData::oneWhole(const Run& run, int hour, const std::string& file, const Want& want, GfsGrid::Grid& out, std::string& error) const {
    const std::string variable = source.variableOf ? source.variableOf(want) : want.variable;   // what a NetCDF file calls the field
    const int timeBand = source.ncBand ? source.ncBand(run, hour, want) : 0;
    QString name = QString::fromStdString(source.id + "_" + run.id() + "_f" + pad(hour, 3) + "_" + file + "_" + ((source.netcdf || source.custom) ? variable : std::string{}) + (timeBand > 0 ? "_b" + std::to_string(timeBand) : std::string{}) + "_");
    name.replace(QRegularExpression{"[^A-Za-z0-9_.-]"}, "-");
    if (name.size() > 110) {
        name = name.left(80) + "_" + QString::number(qHash(name), 16) + "_";
    }
    const QString wholePath = config.cacheFolder + "/" + name + "all.grb2";
    const QString statName = QString::fromStdString(want.stat.empty() ? "mean" : want.stat);
    const QString cachePath = config.cacheFolder + "/" + name + statName + ".gz4";
    QDir{}.mkpath(config.cacheFolder);
    constexpr int header = 4 + 4 + 8 + 8 + 8;
    {
        QFile cached{cachePath};
        if (cached.open(QIODevice::ReadOnly)) {
            const auto all = cached.readAll();
            GfsGrid::Grid g;
            if (all.size() > header) {
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
    }
    if (source.partial) {   // the part of the file that is wanted, from a server that gives it: kept like the others; when it cannot, the file is read as usual
        GfsGrid::Grid g;
        std::string why;
        if (source.partial(run, hour, file, variable, g, why) && !g.values.empty()) {
            const double factor = source.valueScale ? source.valueScale(want) : 1.0;
            const double offset = source.valueOffset ? source.valueOffset(want) : 0.0;
            for (auto& v : g.values) {
                v = static_cast<float>(v * factor + offset);
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
    }
    if (source.custom) {   // the registered loader makes the grid; it is kept like the others
        Loader loader;
        {
            const std::lock_guard lock{loaderMutex};
            const auto found = loaders().find(source.id);
            if (found != loaders().end()) {
                loader = found->second;
            }
        }
        if (!loader) {
            error = source.id + " is not available in this build";
            return false;
        }
        GfsGrid::Grid g;
        if (!loader(run, hour, want, g, error)) {
            return false;
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
    const int band = timeBand > 0 ? timeBand : source.memberBand && !want.stat.empty() && want.stat != "spr" ? source.memberBand(want.stat) : 0;
    if (!want.stat.empty() && want.stat != "spr" && band <= 0) {
        error = source.id + " has no member " + want.stat;
        return false;
    }
    // the first place the field is: the kept file, or the first of the urls that answers
    QString input = wholePath;
    if (!source.netcdf && QFileInfo::exists(wholePath) && QFileInfo{wholePath}.size() >= 100) {
        // kept
    } else {
        std::string failed;
        bool have = false;
        for (const auto& candidate : urlsFor(run, hour, file)) {
            const auto url = resolveUrl(candidate);
            if (url.empty()) {
                failed = "not in the listing";
                continue;
            }
            if (source.netcdf) {
                const auto head = config.bytes(url, 0, 3);
                if (head.startsWith("CDF") || head.startsWith("\x89HDF")) {
                    input = "NETCDF:\"/vsicurl/" + QString::fromStdString(url) + "\":" + QString::fromStdString(variable);
                    have = true;
                    break;
                }
                continue;
            }
            const auto bytes = config.bytes(url, 0, -1);
            if (bytes.size() < 100 || !bytes.startsWith("GRIB")) {
                continue;
            }
            QSaveFile f{wholePath};
            if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
                error = "could not write " + wholePath.toStdString();
                return false;
            }
            have = true;
            break;
        }
        if (!have) {
            error = source.id + " does not have " + (variable.empty() ? file : variable) + " for " + run.id() + " f" + pad(hour, 3) + (failed.empty() ? "" : " (" + failed + ")");
            return false;
        }
    }
    const auto scratch = name + statName + "_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16) + "_" + QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
    const auto rawPath = config.cacheFolder + "/" + scratch + ".raw";
    const auto hdrPath = config.cacheFolder + "/" + scratch + ".hdr";
    const auto cleanup = [&] {
        QFile::remove(rawPath);
        QFile::remove(hdrPath);
        QFile::remove(rawPath + ".aux.xml");
    };
    QStringList args;
    QString program = "/gdal_translate";
    if (source.warp.enabled) {   // a grid that is not latitude / longitude: warped to one
        program = "/gdalwarp";
        args << "-q" << "-overwrite" << "-t_srs" << "EPSG:4326" << "-r" << "bilinear" << "-dstnodata" << "-9999" << "-of" << "ENVI" << "-ot" << "Float32"
             << "-te" << QString::number(source.warp.west, 'f', 3) << QString::number(source.warp.south, 'f', 3) << QString::number(source.warp.east, 'f', 3) << QString::number(source.warp.north, 'f', 3)
             << "-tr" << QString::number(source.warp.step, 'f', 4) << QString::number(source.warp.step, 'f', 4);
        if (band > 0) {
            args << "-srcband" << QString::number(band);
        }
    } else {
        args << "-q" << "-of" << "ENVI" << "-ot" << "Float32";
        if (band > 0) {
            args << "-b" << QString::number(band);
        }
    }
    if (source.netcdf) {   // by range: a few requests of a megabyte or so, not the file
        args << "--config" << "CPL_VSIL_CURL_CHUNK_SIZE" << "2097152" << "--config" << "GDAL_HTTP_USERAGENT" << "wxqt";
    }
    args << input << rawPath;
    QProcess translate;
    translate.start(QString::fromStdString(config.gdalBin) + program, args);
    translate.waitForFinished(300000);
    if (translate.exitStatus() != QProcess::NormalExit || translate.exitCode() != 0) {
        error = "gdal failed on " + want.variable + " " + want.level + ": " + translate.readAllStandardError().trimmed().toStdString();
        cleanup();
        return false;
    }
    GfsGrid::Grid g;
    int bands = 0;
    std::vector<float> all;
    const bool read = readEnvi(rawPath, hdrPath, g, bands, all, error);
    cleanup();
    if (!read) {
        return false;
    }
    const size_t cells = static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows);
    g.values.assign(cells, std::nanf(""));
    const double factor = source.valueScale ? source.valueScale(want) : 1.0;
    const double offset = source.valueOffset ? source.valueOffset(want) : 0.0;
    for (size_t i = 0; i < cells; i++) {
        double sum = 0.0, square = 0.0;
        int n = 0;
        for (int b = 0; b < bands; b++) {
            const float v = all[static_cast<size_t>(b) * cells + i];
            if (std::abs(v) < 1e19f && v == v && v != source.extraMissing && !(source.warp.enabled && v < -9998.5f)) {
                sum += v;
                square += static_cast<double>(v) * v;
                n++;
            }
        }
        if (n == 0) {
            continue;
        }
        const double mean = sum / n;
        const double value = want.stat == "spr" ? std::sqrt(std::max(0.0, square / n - mean * mean)) : mean;
        g.values[i] = static_cast<float>(value * factor + (want.stat == "spr" ? 0.0 : offset));
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

void GfsData::setLoader(const std::string& sourceId, Loader loader) {
    const std::lock_guard lock{loaderMutex};
    loaders()[sourceId] = std::move(loader);
}

bool GfsData::check(const Run& run, const std::vector<Need>& needs, std::string& detail) const {
    if (needs.empty()) {
        detail = "no chart to check";
        return false;
    }
    if (!source.forecastText.empty() && std::any_of(needs.begin(), needs.end(), [] (const Need& n) { return n.want.forecast.empty(); })) {   // the hour is a record of the file, not a file
        auto fixed = needs;
        for (auto& n : fixed) {
            if (n.want.forecast.empty()) {
                n.want.forecast = source.forecastText;
                n.want.forecast.replace(n.want.forecast.find("{hour}"), 6, std::to_string(n.hour));
            }
        }
        return check(run, fixed, detail);
    }

    if (source.custom) {
        const std::lock_guard lock{loaderMutex};
        const bool have = loaders().count(source.id) > 0;
        detail = have ? "store found (arrays not checked)" : "no reader registered in this build";
        return have;
    }
    std::set<std::pair<int, std::string>> files;
    for (const auto& need : needs) {
        files.insert({need.hour, fileFor(need.want)});
    }
    if (source.wholeFile) {
        for (const auto& [hour, file] : files) {
            bool there = false;
            for (const auto& candidate : urlsFor(run, hour, file)) {
                const auto url = resolveUrl(candidate);
                if (url.empty()) {
                    continue;
                }
                const auto head = config.bytes(url, 0, 3);
                if (head.startsWith("GRIB") || head.startsWith("CDF") || head.startsWith("\x89HDF")) {
                    there = true;
                    break;
                }
            }
            if (!there) {
                detail = "f" + pad(hour, 3) + (file.empty() ? "" : " " + file) + " is not there";
                return false;
            }
        }
        detail = source.netcdf ? "files found (variables not checked)" : "files found";
        return true;
    }
    for (const auto& [hour, file] : files) {
        const auto bytes = config.bytes(indexUrl(run, hour, file), 0, -1).toStdString();
        Want first;
        for (const auto& need : needs) {
            if (need.hour == hour && fileFor(need.want) == file) {
                first = need.want;
                break;
            }
        }
        const auto index = source.parseIndex ? source.parseIndex(bytes, first) : GfsGrid::parseIdx(bytes);
        if (index.empty()) {
            detail = "no index for f" + pad(hour, 3) + (file.empty() ? "" : " (" + file + ")");
            return false;
        }
        for (const auto& need : needs) {
            if (need.hour != hour || fileFor(need.want) != file || (hour == 0 && !need.want.forecast.empty())) {
                continue;
            }
            const auto& w = need.want;
            if (!GfsGrid::find(index, w.variable, w.level, w.forecast, w.detail.empty() ? source.defaultDetail : w.detail, source.matchRunTime ? run.id() : std::string{})) {
                detail = "no " + w.variable + " " + w.level + (w.forecast.empty() ? "" : " (" + w.forecast + ")") + " in f" + pad(hour, 3);
                return false;
            }
        }
    }
    detail = "index lists the chart's fields";
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
        const int priority = NetPriority::current;   // the requests made on these threads go with the priority and the owner of the caller
        const void * who = NetPriority::owner;
        const size_t workers = std::min<size_t>(static_cast<size_t>(std::max(limit, 1)), count);
        std::vector<std::future<void>> running;
        for (size_t w = 0; w < workers; w++) {
            running.push_back(std::async(std::launch::async, [&] {
                const NetPriority::Carry carry{priority, who};
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

namespace {
    std::mutex indexMutex;
    std::map<std::string, std::pair<std::chrono::steady_clock::time_point, std::vector<GfsGrid::IdxRecord>>> indexCache;
}

namespace {
    // the short name GDAL gives a band's level ("500-ISBL"), from the index's way of writing it ("500 mb"); "" for a level not known (that field goes the usual way)
    std::string bandLevel(const std::string& level) {
        if (level.size() > 3 && level.compare(level.size() - 3, 3, " mb") == 0 && std::all_of(level.begin(), level.end() - 3, [] (unsigned char ch) { return std::isdigit(ch); })) {
            return level.substr(0, level.size() - 3) + "-ISBL";
        }
        if (level == "2 m above ground") return "2-HTGL";
        if (level == "10 m above ground") return "10-HTGL";
        if (level == "mean sea level") return "0-MSL";
        if (level == "surface") return "0-SFC";
        return {};
    }

    bool readCachedGrid(const QString& path, GfsGrid::Grid& g) {
        constexpr int header = 4 + 4 + 8 + 8 + 8;
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const auto all = file.readAll();
        if (all.size() <= header) {
            return false;
        }
        std::memcpy(&g.columns, all.constData(), 4);
        std::memcpy(&g.rows, all.constData() + 4, 4);
        std::memcpy(&g.lon0, all.constData() + 8, 8);
        std::memcpy(&g.lat0, all.constData() + 16, 8);
        std::memcpy(&g.step, all.constData() + 24, 8);
        const auto floats = qUncompress(all.mid(header));
        if (g.columns <= 0 || g.rows <= 0 || floats.size() != static_cast<qsizetype>(g.columns) * g.rows * 4) {
            return false;
        }
        g.values.resize(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows));
        std::memcpy(g.values.data(), floats.constData(), static_cast<size_t>(floats.size()));
        return true;
    }

    void writeCachedGrid(const QString& path, const GfsGrid::Grid& g) {
        constexpr int header = 4 + 4 + 8 + 8 + 8;
        QByteArray store(header, '\0');
        std::memcpy(store.data(), &g.columns, 4);
        std::memcpy(store.data() + 4, &g.rows, 4);
        std::memcpy(store.data() + 8, &g.lon0, 8);
        std::memcpy(store.data() + 16, &g.lat0, 8);
        std::memcpy(store.data() + 24, &g.step, 8);
        store += qCompress(QByteArray::fromRawData(reinterpret_cast<const char *>(g.values.data()), static_cast<qsizetype>(g.values.size() * 4)), 6);
        QSaveFile keep{path};
        if (keep.open(QIODevice::WriteOnly)) {
            keep.write(store);
            keep.commit();
        }
    }
}

// The fields of one forecast hour from a box request. Every field must be one that can be named to the server (an instantaneous field at a level GDAL names the same way); the file that
// comes back holds the messages of every variable x level asked for, and each field is picked out by its variable and level. false: the caller goes the usual way for all of them.
bool GfsData::loadBox(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out) const {
    if (!boxEnabled || needs.empty()) {
        return false;
    }
    std::map<int, std::vector<const Need *>> byHour;
    for (const auto& need : needs) {
        if (!need.want.forecast.empty() || !need.want.detail.empty() || !need.want.stat.empty() || bandLevel(need.want.level).empty() || fileFor(need.want).size() > 0) {
            return false;   // an accumulation, a statistic, a level GDAL names otherwise: not for the box
        }
        byHour[need.hour].push_back(&need);
    }
    const QString boxTag = QString{"_b%1_%2_%3_%4"}.arg(static_cast<int>(box.west)).arg(static_cast<int>(box.south)).arg(static_cast<int>(box.east)).arg(static_cast<int>(box.north));
    const auto cachePath = [&] (const Need& need) {
        QString name = QString::fromStdString(source.id + "_" + run.id() + "_f" + pad(need.hour, 3) + "__" + need.want.variable + "_" + need.want.level + "___p") + boxTag;
        name.replace(QRegularExpression{"[^A-Za-z0-9_.-]"}, "-");
        return config.cacheFolder + "/" + name + ".gz4";
    };
    std::vector<std::pair<int, std::vector<const Need *>>> hours(byHour.begin(), byHour.end());
    std::vector<std::map<std::string, GfsGrid::Grid>> parts(hours.size());
    std::vector<bool> ok(hours.size(), true);
    runLimited(hours.size(), std::min(source.maxParallel, 4), [&] (size_t i) {
        const int hour = hours[i].first;
        std::vector<const Need *> missing;
        for (const auto * need : hours[i].second) {
            GfsGrid::Grid g;
            if (readCachedGrid(cachePath(*need), g)) {
                parts[i][need->want.key] = std::move(g);
            } else {
                missing.push_back(need);
            }
        }
        if (missing.empty()) {
            return;
        }
        std::vector<std::string> variables, levels;
        for (const auto * need : missing) {
            if (std::find(variables.begin(), variables.end(), need->want.variable) == variables.end()) {
                variables.push_back(need->want.variable);
            }
            if (std::find(levels.begin(), levels.end(), need->want.level) == levels.end()) {
                levels.push_back(need->want.level);
            }
        }
        const auto bytes = config.bytes(source.subsetUrl(run, hour, variables, levels, box), 0, -1);
        if (bytes.size() < 100 || !bytes.startsWith("GRIB")) {
            ok[i] = false;   // the server said no (a busy server, a run it does not hold any more)
            return;
        }
        const QString base = config.cacheFolder + "/box_" + QString::fromStdString(run.id()) + "_f" + QString::number(hour) + "_" + QString::number(reinterpret_cast<quintptr>(&hours[i]), 16) + "_" +
            QString::number(QDateTime::currentMSecsSinceEpoch(), 16);
        const QString gribPath = base + ".grb2";
        {
            QFile f{gribPath};
            if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) {
                ok[i] = false;
                return;
            }
        }
        // which band is which: GDAL names each message's variable and level
        QProcess info;
        info.start(QString::fromStdString(config.gdalBin) + "/gdalinfo", {"-json", gribPath});
        info.waitForFinished(60000);
        std::map<std::string, int> bandOf;   // "HGT 500-ISBL" -> band number
        const auto document = QJsonDocument::fromJson(info.readAllStandardOutput());
        for (const auto& b : document.object().value("bands").toArray()) {
            const auto metadata = b.toObject().value("metadata").toObject().value("").toObject();
            const auto key = metadata.value("GRIB_ELEMENT").toString().toStdString() + " " + metadata.value("GRIB_SHORT_NAME").toString().toStdString();
            if (!bandOf.count(key)) {
                bandOf[key] = b.toObject().value("band").toInt();
            }
        }
        for (const auto * need : missing) {
            const auto found = bandOf.find(need->want.variable + " " + bandLevel(need->want.level));
            if (found == bandOf.end()) {
                ok[i] = false;
                break;
            }
            const QString rawPath = base + "_" + QString::number(found->second) + ".raw", hdrPath = base + "_" + QString::number(found->second) + ".hdr";
            QProcess translate;
            translate.start(QString::fromStdString(config.gdalBin) + "/gdal_translate", {"-q", "-b", QString::number(found->second), "-of", "ENVI", "-ot", "Float32", gribPath, rawPath});
            translate.waitForFinished(120000);
            QFile hdr{hdrPath}, raw{rawPath};
            const auto hdrText = hdr.open(QIODevice::ReadOnly) ? QString::fromUtf8(hdr.readAll()) : QString{};
            const auto data = raw.open(QIODevice::ReadOnly) ? raw.readAll() : QByteArray{};
            QFile::remove(rawPath);
            QFile::remove(hdrPath);
            QFile::remove(rawPath + ".aux.xml");
            const auto number = [&hdrText] (const QString& key) {
                const auto m = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(hdrText);
                return m.hasMatch() ? m.captured(1).toDouble() : -1e9;
            };
            const auto info2 = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(hdrText);
            GfsGrid::Grid g;
            g.columns = static_cast<int>(number("samples"));
            g.rows = static_cast<int>(number("lines"));
            if (g.columns <= 0 || g.rows <= 0 || !info2.hasMatch() || info2.captured(3).toDouble() <= 0.0 || data.size() != static_cast<qsizetype>(g.columns) * g.rows * 4) {
                ok[i] = false;
                break;
            }
            g.step = info2.captured(3).toDouble();
            g.lon0 = info2.captured(1).toDouble() + g.step / 2.0;
            g.lat0 = info2.captured(2).toDouble() - g.step / 2.0;
            g.values.resize(static_cast<size_t>(g.columns) * static_cast<size_t>(g.rows));
            std::memcpy(g.values.data(), data.constData(), static_cast<size_t>(data.size()));
            if (QRegularExpression{"byte order\\s*=\\s*1"}.match(hdrText).hasMatch()) {
                for (auto& v : g.values) {
                    unsigned char b[4];
                    std::memcpy(b, &v, 4);
                    std::swap(b[0], b[3]);
                    std::swap(b[1], b[2]);
                    std::memcpy(&v, b, 4);
                }
            }
            for (auto& v : g.values) {
                if (std::abs(v) > 1e19f) {
                    v = std::nanf("");
                }
            }
            writeCachedGrid(cachePath(*need), g);
            parts[i][need->want.key] = std::move(g);
        }
        QFile::remove(gribPath);
    });
    for (size_t i = 0; i < hours.size(); i++) {
        if (!ok[i]) {
            return false;
        }
    }
    for (auto& part : parts) {
        for (auto& [key, grid] : part) {
            out[key] = std::move(grid);
        }
    }
    return true;
}

namespace {
    struct Result {
        GfsGrid::Grid grid;
        std::string error;
        bool ok{false};
        bool absent{false};
    };
}

bool GfsData::load(const Run& run, const std::vector<Need>& needs, std::map<std::string, GfsGrid::Grid>& out, std::string& error) const {
    if (!source.forecastText.empty() && std::any_of(needs.begin(), needs.end(), [] (const Need& n) { return n.want.forecast.empty(); })) {   // the hour is a record of the file, not a file
        auto fixed = needs;
        for (auto& n : fixed) {
            if (n.want.forecast.empty()) {
                n.want.forecast = source.forecastText;
                n.want.forecast.replace(n.want.forecast.find("{hour}"), 6, std::to_string(n.hour));
            }
        }
        return load(run, fixed, out, error);
    }
    if (boxEnabled) {   // a box of the fields when they can all be had that way: a few per cent of the download
        std::map<std::string, GfsGrid::Grid> boxed;
        if (loadBox(run, needs, boxed)) {
            for (auto& [key, grid] : boxed) {
                out[key] = std::move(grid);
            }
            return true;
        }
    }
    // one index per forecast hour and file involved: fetched together (they do not depend on each other), a few at a time
    // (an index that depends on the member, ECMWF's, is one for each member asked for)
    using IndexKey = std::tuple<int, std::string, std::string>;
    const auto keyOf = [this] (const Need& need) { return IndexKey{need.hour, fileFor(need.want), source.parseIndex ? need.want.stat : std::string{}}; };
    std::vector<IndexKey> keys;
    for (const auto& need : needs) {
        const auto key = keyOf(need);
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            keys.push_back(key);
        }
    }
    std::vector<Want> wantsOf(keys.size());   // a want of each index: the member to read
    for (size_t i = 0; i < keys.size(); i++) {
        for (const auto& need : needs) {
            if (keyOf(need) == keys[i]) {
                wantsOf[i] = need.want;
                break;
            }
        }
    }
    if (source.wholeFile) {   // no index to read: each want is a file of its own, and the file holds the members
        std::vector<Result> whole(needs.size());
        runLimited(needs.size(), source.maxParallel, [&] (size_t i) {
            whole[i].ok = oneWhole(run, needs[i].hour, fileFor(needs[i].want), needs[i].want, whole[i].grid, whole[i].error);
        });
        bool every = true;
        for (size_t i = 0; i < whole.size(); i++) {
            if (whole[i].ok) {
                out[needs[i].want.key] = std::move(whole[i].grid);
            } else if (every) {
                error = whole[i].error;
                every = false;
            }
        }
        return every;
    }
    std::vector<std::vector<GfsGrid::IdxRecord>> parsed(keys.size());
    runLimited(keys.size(), source.maxParallel, [&] (size_t i) {
        // an index does not change once the run has its file: kept for a few minutes (it is a request of its own, 1.6 s from ECMWF), so drawing the next hour or chart starts at once
        const auto url = indexUrl(run, std::get<0>(keys[i]), std::get<1>(keys[i]));
        const auto cacheKey = url + "|" + std::get<2>(keys[i]);
        const auto now = std::chrono::steady_clock::now();
        {
            const std::lock_guard lock{indexMutex};
            const auto found = indexCache.find(cacheKey);
            if (found != indexCache.end() && now - found->second.first < std::chrono::minutes(5)) {
                parsed[i] = found->second.second;
                return;
            }
        }
        const auto idxBytes = config.bytes(url, 0, -1);
        parsed[i] = source.parseIndex ? source.parseIndex(idxBytes.toStdString(), wantsOf[i]) : GfsGrid::parseIdx(idxBytes.toStdString());
        if (!parsed[i].empty()) {
            const std::lock_guard lock{indexMutex};
            if (indexCache.size() > 400) {
                indexCache.clear();
            }
            indexCache[cacheKey] = {now, parsed[i]};
        }
    });
    std::map<IndexKey, std::vector<GfsGrid::IdxRecord>> indexes;
    for (size_t i = 0; i < keys.size(); i++) {
        if (parsed[i].empty()) {
            error = "could not read the " + source.id + " index for " + run.id() + " f" + pad(std::get<0>(keys[i]), 3) + (std::get<1>(keys[i]).empty() ? "" : " (" + std::get<1>(keys[i]) + ")");
            return false;
        }
        indexes[keys[i]] = std::move(parsed[i]);
    }
    std::vector<Result> results(needs.size());
    runLimited(needs.size(), source.maxParallel, [&] (size_t i) {   // the downloads and the decoding, a few at a time
        const auto& need = needs[i];
        Result& r = results[i];
        const auto file = fileFor(need.want);
        const auto& index = indexes.at(keyOf(need));
        if (need.hour == 0 && !GfsGrid::find(index, need.want.variable, need.want.level, need.want.forecast, need.want.detail.empty() ? source.defaultDetail : need.want.detail, source.matchRunTime ? run.id() : std::string{})) {
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
