// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mrms/UtilityMrms.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <numbers>
#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTimeZone>
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "util/CrashLog.h"
#include "radar/RadarGeomInfo.h"

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
    // the list heading, and the US-units conversion, of a hand-set product
    vector<UtilityMrms::Product> decorated(vector<UtilityMrms::Product> list) {
        for (auto& p : list) {
            const auto starts = [&p] (const char * prefix) { return p.id.rfind(prefix, 0) == 0; };
            if (starts("MergedReflectivity") || starts("ReflectivityAtLowest")) {
                p.group = "Reflectivity";
            } else if (starts("MESH")) {
                p.group = "Hail";
                p.usFactor = 0.0393701;
                p.usUnits = "in";
            } else if (starts("RotationTrack") || starts("MergedAzShear")) {
                p.group = "Rotation";
            } else if (starts("PrecipRate")) {
                p.group = "Precipitation";
                p.usFactor = 0.0393701;
                p.usUnits = "in/h";
            } else if (starts("MultiSensor_QPE")) {
                p.group = "Precipitation";
                p.usFactor = 0.0393701;
                p.usUnits = "in";
            } else if (starts("VIL")) {
                p.group = "Storm structure";
            } else if (starts("EchoTop")) {
                p.group = "Storm structure";
                p.usFactor = 3.28084;
                p.usUnits = "kft";
            } else {
                p.group = "Lightning";
            }
        }
        return list;
    }

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
    static const vector<P> all = decorated(vector<P>{
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
        P{"RotationTrack60min", "Rotation track, 1 hour", "1e-3/s", 2.0, 30.0,
          {{2, 120, 200, 255}, {6, 0, 220, 0}, {10, 255, 255, 0}, {15, 255, 165, 0}, {20, 255, 0, 0}, {30, 255, 0, 255}}},
        P{"RotationTrack1440min", "Rotation track, 24 hours", "1e-3/s", 2.0, 30.0,
          {{2, 120, 200, 255}, {6, 0, 220, 0}, {10, 255, 255, 0}, {15, 255, 165, 0}, {20, 255, 0, 0}, {30, 255, 0, 255}}},
        P{"MergedAzShear_0-2kmAGL", "Azimuthal shear 0-2 km", "1e-3/s", 2.0, 30.0,
          {{2, 120, 200, 255}, {6, 0, 220, 0}, {10, 255, 255, 0}, {15, 255, 165, 0}, {20, 255, 0, 0}, {30, 255, 0, 255}}},
        P{"MergedAzShear_3-6kmAGL", "Azimuthal shear 3-6 km", "1e-3/s", 2.0, 30.0,
          {{2, 120, 200, 255}, {6, 0, 220, 0}, {10, 255, 255, 0}, {15, 255, 165, 0}, {20, 255, 0, 0}, {30, 255, 0, 255}}},
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
    });
    return all;
}

QVector<QRgb> UtilityMrms::colorTable(const Product& product, const Frame& frame) {
    QVector<QRgb> table(256, qRgba(0, 0, 0, 0));
    const double lo = frame.validMin;
    const double span = frame.step * 254.0;
    for (int index = 1; index < 256; index += 1) {
        const double value = lo + (index - 1) * frame.step;
        // an automatic scale spans the scan's own range: its stops are fractions of it
        vector<Stop> scaled;
        if (product.autoRange) {
            for (auto stop : product.stops) {
                stop.value = lo + stop.value * span;
                scaled.push_back(stop);
            }
        }
        const auto& stops = product.autoRange ? scaled : product.stops;
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

double UtilityMrms::shown(const Product& product, double value, bool us) {
    return us ? value * product.usFactor : value;
}

string UtilityMrms::unitsShown(const Product& product, bool us) {
    return us && !product.usUnits.empty() ? product.usUnits : product.units;
}

// every other folder the server has: generic colours spanning each scan's own range
bool UtilityMrms::discoverMore(vector<Product>& out, string& error) {
    out.clear();
    const auto listing = URL::getText(site);
    if (listing.empty()) {
        error = "no answer from mrms.ncep.noaa.gov";
        return false;
    }
    static const vector<string> skip{"ALASKA", "CARIB", "GUAM", "HAWAII", "FLASH"};   // other grids / sub-folders
    auto matches = QRegularExpression{"href=\"([A-Za-z0-9_.-]+)/\""}.globalMatch(QString::fromStdString(listing));
    vector<string> seen;
    while (matches.hasNext()) {
        const auto id = matches.next().captured(1).toStdString();
        const auto known = std::any_of(products().begin(), products().end(), [&id] (const Product& p) { return p.id == id; });
        if (known || std::find(skip.begin(), skip.end(), id) != skip.end() || std::find(seen.begin(), seen.end(), id) != seen.end()) {
            continue;
        }
        seen.push_back(id);
        auto label = id;
        std::replace(label.begin(), label.end(), '_', ' ');
        Product product{id, label, "", -90.0, 0.0,
                        {{0.0, 120, 200, 255}, {0.25, 0, 200, 0}, {0.5, 255, 255, 0}, {0.75, 255, 165, 0}, {1.0, 255, 0, 0}}};
        product.group = "Other products";
        product.autoRange = true;
        out.push_back(product);
    }
    std::sort(out.begin(), out.end(), [] (const Product& a, const Product& b) { return a.label < b.label; });
    return true;
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

namespace {
    bool frameOnce(const UtilityMrms::Product&, const UtilityMrms::Scan&, UtilityMrms::Frame&, string&);
}

// a scan that fails to decode is fetched afresh once (a truncated or odd download looks the same as a real fault)
bool UtilityMrms::frame(const Product& product, const Scan& scan, Frame& out, string& error) {
    // one decode at a time: two of the same scan (viewer, thumbnail, auto-update) would delete each other's scratch files
    static std::mutex decodeMutex;
    const std::lock_guard<std::mutex> lock{decodeMutex};
    if (frameOnce(product, scan, out, error)) {
        return true;
    }
    CrashLog::write("MRMS: retrying " + scan.file + " after: " + error);
    QFile::remove(QString::fromStdString(pathFor("mrmsg_" + scan.file)));
    error.clear();
    return frameOnce(product, scan, out, error);
}

namespace {
bool frameOnce(const UtilityMrms::Product& product, const UtilityMrms::Scan& scan, UtilityMrms::Frame& out, string& error) {
    using namespace UtilityMrms;
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        error = "GDAL not found - install the 'gdal' package";
        return false;
    }
    out.utc = scan.utc;
    // the cache file is the scan's scale and grid followed by the compressed indices: value of index 1 and step (2 doubles),
    // columns and rows (2 ints), west, north and cell size (3 doubles)
    constexpr int headerSize = 8 + 8 + 4 + 4 + 8 + 8 + 8;
    const auto cachePath = QString::fromStdString(pathFor("mrmsd4_" + scan.file + ".u8z"));
    QFile cached{cachePath};
    if (cached.exists() && cached.open(QIODevice::ReadOnly)) {
        const auto all = cached.readAll();
        cached.close();
        if (all.size() > headerSize) {
            const auto packed = all.mid(headerSize);
            Grid grid;
            std::memcpy(&out.validMin, all.constData(), 8);
            std::memcpy(&out.step, all.constData() + 8, 8);
            std::memcpy(&grid.columns, all.constData() + 16, 4);
            std::memcpy(&grid.rows, all.constData() + 20, 4);
            std::memcpy(&grid.west, all.constData() + 24, 8);
            std::memcpy(&grid.north, all.constData() + 32, 8);
            std::memcpy(&grid.cell, all.constData() + 40, 8);
            if (grid.columns > 0 && grid.rows > 0 && qUncompress(packed).size() == static_cast<qsizetype>(grid.columns) * grid.rows) {
                out.grid = grid;
                out.packed = packed;
                return true;
            }
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
    // the picture is placed by the scan's own grid: read it from the header, refuse anything that is not a plain
    // north-up latitude / longitude grid
    QFile header{hdrPath};
    const auto headerText = header.open(QIODevice::ReadOnly) ? QString::fromUtf8(header.readAll()) : QString{};
    const auto number = [&headerText] (const QString& key) {
        const auto match = QRegularExpression{"\\b" + key + "\\s*=\\s*([-0-9.]+)"}.match(headerText);
        return match.hasMatch() ? match.captured(1).toDouble() : -1e9;
    };
    const auto mapInfo = QRegularExpression{"map info = \\{[^,]*,\\s*1,\\s*1,\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+),\\s*([-0-9.]+)"}.match(headerText);
    Grid grid;
    grid.columns = static_cast<int>(number("samples"));
    grid.rows = static_cast<int>(number("lines"));
    const bool gridOk = grid.columns > 0 && grid.rows > 0 && mapInfo.hasMatch() && mapInfo.captured(3).toDouble() > 0.0 &&
        std::abs(mapInfo.captured(3).toDouble() - mapInfo.captured(4).toDouble()) < 1e-6;
    if (gridOk) {
        grid.west = mapInfo.captured(1).toDouble();
        grid.north = mapInfo.captured(2).toDouble();
        grid.cell = mapInfo.captured(3).toDouble();
    }
    if (!gridOk) {
        error = scan.file + " is not on a plain latitude / longitude grid";
        CrashLog::write("MRMS: " + error + " - header was: " + headerText.left(600).replace('\n', ' ').toStdString());
        QFile::remove(gzPath);
        cleanup();
        return false;
    }
    out.grid = grid;
    const auto cells = static_cast<qsizetype>(grid.columns) * grid.rows;
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
    // the scale: hand-set for most products, else the scan's own range
    double lo = product.validMin;
    double hi = product.hi;
    if (product.autoRange) {
        double minimum = 1e30;
        double maximum = -1e30;
        for (qsizetype i = 0; i < cells; i += 1) {
            const double v = values[i];
            if (!std::isnan(v) && v > product.validMin) {
                minimum = std::min(minimum, v);
                maximum = std::max(maximum, v);
            }
        }
        if (maximum < minimum) {   // nothing in the scan
            minimum = 0.0;
            maximum = 1.0;
        }
        lo = minimum;
        hi = maximum;
        if (hi - lo > 1e-9) {
            // a few extreme cells would stretch the scale until everything else is one colour: span the 1st - 99th percentile
            constexpr int bins = 2048;
            vector<int> histogram(bins, 0);
            long long total = 0;
            for (qsizetype i = 0; i < cells; i += 1) {
                const double v = values[i];
                if (!std::isnan(v) && v > product.validMin) {
                    histogram[std::clamp(static_cast<int>((v - minimum) / (maximum - minimum) * (bins - 1)), 0, bins - 1)] += 1;
                    total += 1;
                }
            }
            long long running = 0;
            int lowBin = 0;
            int highBin = bins - 1;
            for (int b = 0; b < bins; b += 1) {
                running += histogram[b];
                if (running <= total / 100) {
                    lowBin = b;
                }
                if (running < total - total / 100) {
                    highBin = b + 1;
                }
            }
            lo = minimum + (maximum - minimum) * lowBin / (bins - 1);
            hi = minimum + (maximum - minimum) * std::min(highBin, bins - 1) / (bins - 1);
        }
        if (hi - lo < 1e-9) {
            hi = lo + 1.0;
        }
    }
    const double step = (hi - lo) / 254.0;
    out.validMin = lo;
    out.step = step;
    QByteArray indices(cells, 0);
    auto * dst = reinterpret_cast<uchar *>(indices.data());
    for (qsizetype i = 0; i < cells; i += 1) {
        const double v = values[i];
        if (!std::isnan(v) && v > (product.autoRange ? product.validMin : lo - 1e-12)) {
            dst[i] = static_cast<uchar>(std::clamp(1 + static_cast<int>(std::lround((v - lo) / step)), 1, 255));
        }
    }
    out.packed = qCompress(indices, 1);
    QFile cache{cachePath};
    if (cache.open(QIODevice::WriteOnly)) {
        QByteArray header(headerSize, 0);
        std::memcpy(header.data(), &lo, 8);
        std::memcpy(header.data() + 8, &step, 8);
        std::memcpy(header.data() + 16, &grid.columns, 4);
        std::memcpy(header.data() + 20, &grid.rows, 4);
        std::memcpy(header.data() + 24, &grid.west, 8);
        std::memcpy(header.data() + 32, &grid.north, 8);
        std::memcpy(header.data() + 40, &grid.cell, 8);
        cache.write(header + out.packed);
    }
    QFile::remove(gzPath);   // the decoded grid is what is kept
    return true;
}
}   // namespace

// a picture of a latitude / longitude box (equal degrees of longitude and of Mercator latitude per pixel, like the radar
// screens) from the newest scan: the grid sampled per pixel on white, with state lines and, in a small box, counties
bool UtilityMrms::thumbnail(const Product& product, double latSouth, double latNorth, double lonWest, double lonEast,
                            int width, QByteArray& png, string& error) {
    png.clear();
    vector<Scan> list;
    Frame scan;
    if (!scans(product, list, error) || !frame(product, list.back(), scan, error)) {
        return false;
    }
    const auto mercator = [] (double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    };
    const double mercatorSouth = mercator(latSouth);
    const double mercatorNorth = mercator(latNorth);
    const double lonSpan = lonEast - lonWest;
    const int height = std::max(1, static_cast<int>(std::lround(width * (mercatorNorth - mercatorSouth) / lonSpan)));
    const auto cells = scan.indices();
    const auto colors = colorTable(product, scan);
    QImage image{width, height, QImage::Format_RGB32};
    image.fill(Qt::white);
    const auto * source = reinterpret_cast<const uchar *>(cells.constData());
    for (int j = 0; j < height; j += 1) {
        const double m = mercatorNorth - (j + 0.5) / height * (mercatorNorth - mercatorSouth);
        const double lat = std::atan(std::sinh(m * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        const int row = static_cast<int>(std::floor((scan.grid.north - lat) / scan.grid.cell));
        if (row < 0 || row >= scan.grid.rows) {
            continue;
        }
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
        for (int i = 0; i < width; i += 1) {
            const double lon = lonWest + (i + 0.5) / width * lonSpan;
            const int column = static_cast<int>(std::floor((lon - scan.grid.west) / scan.grid.cell));
            if (column >= 0 && column < scan.grid.columns) {
                const auto index = source[static_cast<qsizetype>(row) * scan.grid.columns + column];
                if (index != 0) {
                    out[i] = colors[index];
                }
            }
        }
    }
    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto drawLines = [&] (RadarGeometryTypeEnum type, const QColor& color, double lineWidth) {
        static std::map<RadarGeometryTypeEnum, vector<float>> loaded;
        auto & data = loaded[type];
        if (data.empty()) {
            RadarGeomInfo::loadData(RadarGeomInfo::typeToFileName.at(type), data);
        }
        painter.setPen(QPen{color, lineWidth});
        QVector<QLineF> lines;
        for (size_t i = 0; i + 3 < data.size(); i += 4) {
            // the line files hold latitude and West-positive longitude, as pairs of end points
            const double lat1 = data[i];
            const double lon1 = -data[i + 1];
            const double lat2 = data[i + 2];
            const double lon2 = -data[i + 3];
            if ((lon1 < lonWest && lon2 < lonWest) || (lon1 > lonEast && lon2 > lonEast) ||
                (lat1 < latSouth && lat2 < latSouth) || (lat1 > latNorth && lat2 > latNorth)) {
                continue;
            }
            lines.push_back({(lon1 - lonWest) / lonSpan * width, (mercatorNorth - mercator(lat1)) / (mercatorNorth - mercatorSouth) * height,
                             (lon2 - lonWest) / lonSpan * width, (mercatorNorth - mercator(lat2)) / (mercatorNorth - mercatorSouth) * height});
        }
        painter.drawLines(lines);
    };
    if (lonSpan <= 16.0) {
        drawLines(CountyLines, QColor{170, 170, 170}, 0.6);
    }
    drawLines(StateLines, QColor{30, 30, 30}, 1.2);
    painter.end();
    QBuffer buffer{&png};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return !png.isEmpty();
}
