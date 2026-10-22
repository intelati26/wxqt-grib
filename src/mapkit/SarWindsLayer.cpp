// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/SarWindsLayer.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <numbers>
#include <QCheckBox>
#include <QDateTime>
#include <QVBoxLayout>
#include <QtEndian>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include "gfs/BarbGrid.h"
#include "hurricane/SarWindsViewer.h"
#include "ui/WindBarb.h"
#include "util/PermanentCache.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "util/PermanentCache.h"
#include "radar/MapView.h"

namespace {
    constexpr double toKnots = 1.943844;

    // the wind in the colors the storm layers use for 1 minute winds, a little finer: calm blue, tropical storm green to yellow, hurricane orange to red, major magenta to white
    QRgb windColor(double knots) {
        static const struct { double kt; int r, g, b; } stops[] = {{0, 30, 70, 190}, {15, 40, 150, 230}, {34, 60, 200, 110}, {50, 240, 225, 60}, {64, 250, 160, 30}, {83, 235, 70, 50}, {96, 205, 40, 150}, {113, 150, 60, 200}, {137, 255, 255, 255}};
        const auto& first = stops[0];
        if (knots <= first.kt) {
            return qRgba(first.r, first.g, first.b, 215);
        }
        for (size_t i = 1; i < std::size(stops); i++) {
            if (knots <= stops[i].kt) {
                const double f = (knots - stops[i - 1].kt) / (stops[i].kt - stops[i - 1].kt);
                return qRgba(static_cast<int>(stops[i - 1].r + (stops[i].r - stops[i - 1].r) * f), static_cast<int>(stops[i - 1].g + (stops[i].g - stops[i - 1].g) * f),
                             static_cast<int>(stops[i - 1].b + (stops[i].b - stops[i - 1].b) * f), 215);
            }
        }
        return qRgba(255, 255, 255, 215);
    }

    constexpr uint16_t noValue = 65535;

    // the picture of a grid of winds (tenths of a knot): the colors; the cells the rotated grid missed are filled from their neighbors
    QImage colorize(const std::vector<uint16_t>& grid, int width, int height) {
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const uint16_t v = grid[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
                if (v != noValue) {
                    image.setPixel(x, y, windColor(v / 10.0));
                }
            }
        }
        return image;
    }

    double mercatorY(double lat) {
        return std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    }

    bool readLayer(const QString& file, const QString& variable, const QString& out, int& columns, int& rows, std::vector<float>& values) {
        QProcess translate;
        translate.start(QString::fromStdString(UtilityGrib::gdalBinDir()) + "/gdal_translate", {"-q", "-of", "ENVI", "-ot", "Float32", "NETCDF:\"" + file + "\":" + variable, out});
        translate.waitForFinished(120000);
        if (translate.exitCode() != 0) {
            return false;
        }
        // the size from the ENVI header
        QFile header{QString{out}.replace(QRegularExpression{"\\.raw$"}, ".hdr")};   // (GDAL names the ENVI header after the file without its extension)
        if (!header.open(QIODevice::ReadOnly)) {
            return false;
        }
        const auto text = QString::fromUtf8(header.readAll());
        columns = rows = 0;
        for (const auto& line : text.split('\n')) {
            if (line.startsWith("samples")) {
                columns = line.section('=', 1).trimmed().toInt();
            } else if (line.startsWith("lines")) {
                rows = line.section('=', 1).trimmed().toInt();
            }
        }
        const bool big = text.contains("byte order = 1");
        QFile raw{out};
        if (columns <= 0 || rows <= 0 || !raw.open(QIODevice::ReadOnly)) {
            return false;
        }
        const auto bytes = raw.readAll();
        if (bytes.size() < static_cast<qsizetype>(columns) * rows * 4) {
            return false;
        }
        values.resize(static_cast<size_t>(columns) * static_cast<size_t>(rows));
        for (size_t i = 0; i < values.size(); i++) {
            uint32_t word;
            std::memcpy(&word, bytes.constData() + i * 4, 4);
            if (big) {
                word = qbswap<quint32>(word);
            }
            std::memcpy(&values[i], &word, 4);
        }
        return true;
    }

    // The scenes already painted, kept until the program is closed: every map window shares them; the 12 newest used stay in memory
    std::mutex memoryMutex;
    std::map<string, std::shared_ptr<void>> memory;   // (a SceneData: the type is the layer's own)
    std::deque<string> memoryOrder;                    // oldest first

    // what is kept on disk of a scene (about a megabyte compressed; the 16 MB NetCDF is not): both grids and where they are
    template <class Scene>
    string encodeScene(const Scene& d) {
        string out{"SAR2"};
        const auto put = [&out] (const void * data, size_t n) { out.append(static_cast<const char *>(data), n); };
        put(&d.width, 4);
        put(&d.height, 4);
        const double numbers[7] = {d.west, d.south, d.east, d.north, d.maxKnots, d.stepRad, d.yTop};
        put(numbers, sizeof(numbers));
        const int corners = static_cast<int>(d.outline.size());
        put(&corners, 4);
        for (const auto& p : d.outline) {
            const double xy[2] = {p.x(), p.y()};
            put(xy, sizeof(xy));
        }
        put(d.wind.data(), d.wind.size() * 2);
        put(d.direction.data(), d.direction.size() * 2);
        return out;
    }

    template <class Scene>
    bool decodeScene(const string& in, Scene& d) {
        if (in.size() < 4 + 8 + 7 * sizeof(double) + 4 || in.compare(0, 4, "SAR2") != 0) {
            return false;
        }
        size_t at = 4;
        const auto get = [&in, &at] (void * data, size_t n) {
            if (at + n > in.size()) {
                return false;
            }
            std::memcpy(data, in.data() + at, n);
            at += n;
            return true;
        };
        double numbers[7];
        int corners = 0;
        if (!get(&d.width, 4) || !get(&d.height, 4) || !get(numbers, sizeof(numbers)) || !get(&corners, 4) || d.width <= 0 || d.height <= 0 || d.width > 4000 || d.height > 4000 || corners < 0 || corners > 16) {
            return false;
        }
        d.west = numbers[0];
        d.south = numbers[1];
        d.east = numbers[2];
        d.north = numbers[3];
        d.maxKnots = numbers[4];
        d.stepRad = numbers[5];
        d.yTop = numbers[6];
        d.outline.clear();
        for (int i = 0; i < corners; i++) {
            double xy[2];
            if (!get(xy, sizeof(xy))) {
                return false;
            }
            d.outline << QPointF{xy[0], xy[1]};
        }
        const size_t cells = static_cast<size_t>(d.width) * static_cast<size_t>(d.height);
        d.wind.resize(cells);
        d.direction.resize(cells);
        if (!get(d.wind.data(), cells * 2) || !get(d.direction.data(), cells * 2)) {
            return false;
        }
        d.image = colorize(d.wind, d.width, d.height);
        return true;
    }
}

bool SarWindsLayer::working() const {
    if (loading) {
        return true;
    }
    if (entries) {
        for (const auto& e : *entries) {
            if (e.rendering) {
                return true;
            }
        }
    }
    return false;
}

string SarWindsLayer::summary() const {
    if (entries) {   // a scene being read or painted: the long wait (16 MB the first time) is said
        for (const auto& e : *entries) {
            if (e.rendering) {
                return "downloading and painting the SAR scene of " + e.stormName + " (" + e.when.toString("MMM d HH:mm").toStdString() + "Z, 16 MB the first time; kept after)";
            }
        }
    }
    if (loading && !entries) return "reading STAR's SAR scenes...";
    if (!error.empty()) return error;
    if (!entries) return {};
    return entries->empty() ? string{"no SAR scene of a storm in the last 3 days"} : std::to_string(entries->size()) + " scenes of the last 3 days";
}

void SarWindsLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<std::vector<Entry>>();
    auto message = std::make_shared<string>();
    host.background(
        [fresh, message] {
            const auto index = UtilitySarWinds::index();
            if (!index.ok) {
                *message = "STAR's SAR winds page could not be read";
                return;
            }
            const auto now = QDateTime::currentDateTimeUtc();
            for (const auto& latest : index.latest) {
                const int year = latest.id.size() >= 8 ? std::atoi(latest.id.substr(latest.id.size() - 4).c_str()) : 0;
                vector<UtilitySarWinds::Scene> scenes;
                UtilitySarWinds::storm(latest.id, latest.name, year, scenes);
                for (const auto& scene : scenes) {
                    auto time = QDateTime::fromString(QString::fromStdString(scene.time).replace("UTC", "").simplified(), "yyyy-MM-dd H:mm:ss");
                    time.setTimeSpec(Qt::UTC);
                    if (!time.isValid() || time < now.addDays(-3) || scene.wind.empty() || scene.lat < -90.0) {
                        continue;
                    }
                    Entry e;
                    e.stormId = latest.id;
                    e.stormName = latest.name;
                    e.year = year;
                    e.scene = scene;
                    e.when = time;
                    e.netcdf = scene.wind;
                    if (const auto at = e.netcdf.rfind("_wind.png"); at != string::npos) {
                        e.netcdf = e.netcdf.substr(0, at) + "_wind_level2.nc";
                    }
                    fresh->push_back(std::move(e));
                }
            }
            std::sort(fresh->begin(), fresh->end(), [] (const Entry& a, const Entry& b) { return a.when > b.when; });
        },
        [this, &host, fresh, message] {
            loading = false;
            if (entries) {   // the scenes already painted are kept when the list is read again
                for (auto& e : *fresh) {
                    for (auto& old : *entries) {
                        if (old.netcdf == e.netcdf && old.data) {
                            e.data = old.data;
                        }
                    }
                }
            }
            entries = fresh;
            error = fresh->empty() ? *message : string{};
            host.redraw();
        });
}

// the scene's wind field: STAR's NetCDF (the wind, its latitude and longitude, the land mask and the direction of the model wind) read with GDAL and put on a regular grid, square in Mercator
void SarWindsLayer::render(MapHost& host, size_t index) {
    auto& entry = (*entries)[index];
    if (entry.rendering || entry.data || entry.failed) {
        return;
    }
    const auto url = entry.netcdf;
    {   // painted before in this session (here or in another window): no work at all
        std::lock_guard<std::mutex> lock{memoryMutex};
        if (const auto found = memory.find(url); found != memory.end()) {
            entry.data = std::static_pointer_cast<SceneData>(found->second);
            memoryOrder.erase(std::remove(memoryOrder.begin(), memoryOrder.end(), url), memoryOrder.end());
            memoryOrder.push_back(url);
            host.redraw();
            return;
        }
    }
    entry.rendering = true;
    auto result = std::make_shared<SceneData>();
    auto ok = std::make_shared<bool>(false);
    auto keep = entries;
    host.background(
        [result, ok, url] {
            const PermanentCache store{"sar"};
            const string name = url.substr(url.rfind('/') + 1) + ".2.bin";
            if (const auto text = store.read(name); !text.empty() && decodeScene(text, *result)) {   // read before, in an earlier session (a scene never changes once STAR has published it)
                *ok = true;
                return;
            }
            if (UtilityGrib::gdalBinDir().empty()) {
                return;
            }
            const auto bytes = URL::getBytes(url);
            if (bytes.size() < 10000) {
                return;
            }
            QTemporaryDir dir;
            if (!dir.isValid()) {
                return;
            }
            const QString file = dir.filePath("scene.nc");
            {
                QFile f{file};
                if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) {
                    return;
                }
            }
            int columns = 0, rows = 0, c = 0, r = 0;
            std::vector<float> wind, lat, lon, mask, direction;
            const auto same = [&] { const bool match = c == columns && r == rows; return match; };
            if (!readLayer(file, "sar_wind", dir.filePath("w.raw"), columns, rows, wind) || !readLayer(file, "latitude", dir.filePath("a.raw"), c, r, lat) || !same() ||
                !readLayer(file, "longitude", dir.filePath("o.raw"), c, r, lon) || !same()) {
                return;
            }
            const bool haveMask = readLayer(file, "mask", dir.filePath("m.raw"), c, r, mask) && same();
            const bool haveDirection = readLayer(file, "input_dir", dir.filePath("d.raw"), c, r, direction) && same();
            double minLon = 1e9, maxLon = -1e9, minLat = 1e9, maxLat = -1e9;
            for (size_t i = 0; i < wind.size(); i++) {
                if (std::isnan(lat[i]) || std::isnan(lon[i]) || std::abs(lat[i]) > 90.0) {
                    continue;
                }
                double lo = lon[i];
                if (lo > 180.0) lo -= 360.0;
                minLon = std::min(minLon, lo);
                maxLon = std::max(maxLon, lo);
                minLat = std::min<double>(minLat, lat[i]);
                maxLat = std::max<double>(maxLat, lat[i]);
            }
            if (maxLon <= minLon || maxLat <= minLat) {
                return;
            }
            // a grid with one cell for about the size of a cell of the scene (a square in Mercator)
            auto& d = *result;
            d.stepRad = std::max(0.004, (maxLon - minLon) / 1400.0) * std::numbers::pi / 180.0;   // radians of longitude per cell
            d.width = std::clamp(static_cast<int>(std::ceil((maxLon - minLon) * std::numbers::pi / 180.0 / d.stepRad)) + 2, 8, 1600);
            d.yTop = mercatorY(maxLat);
            d.height = std::clamp(static_cast<int>(std::ceil((d.yTop - mercatorY(minLat)) / d.stepRad)) + 2, 8, 1600);
            const size_t cells = static_cast<size_t>(d.width) * static_cast<size_t>(d.height);
            std::vector<uint16_t> grid(cells, noValue);
            d.direction.assign(cells, noValue);
            double top = 0.0;
            for (int row = 0; row < rows; row++) {
                for (int column = 0; column < columns; column++) {
                    const size_t i = static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column);
                    const float w = wind[i];
                    // no wind: not measured, land (the retrieval gives a fixed 100 m/s there), or a retrieval that gave up (that same 100)
                    if (std::isnan(w) || w < 0.0f || w >= 99.9f || (haveMask && mask[i] > 0.0f) || std::isnan(lat[i]) || std::abs(lat[i]) > 90.0) {
                        continue;
                    }
                    double lo = lon[i];
                    if (lo > 180.0) lo -= 360.0;
                    const int x = static_cast<int>((lo - minLon) * std::numbers::pi / 180.0 / d.stepRad);
                    const int y = static_cast<int>((d.yTop - mercatorY(lat[i])) / d.stepRad);
                    if (x >= 0 && y >= 0 && x < d.width && y < d.height) {
                        const size_t at = static_cast<size_t>(y) * static_cast<size_t>(d.width) + static_cast<size_t>(x);
                        grid[at] = static_cast<uint16_t>(std::clamp(w * toKnots * 10.0, 0.0, 60000.0));
                        if (haveDirection && direction[i] >= 0.0f && direction[i] <= 360.0f) {
                            d.direction[at] = static_cast<uint16_t>(std::clamp(direction[i] * 10.0f, 0.0f, 3600.0f));
                        }
                        top = std::max(top, static_cast<double>(w * toKnots));
                    }
                }
            }
            // the holes inside the scene (cells the rotated grid missed, a few percent of them): the average of the neighbors that have a wind, in two passes so that a hole beside a hole is filled too
            d.wind = grid;
            for (int pass = 0; pass < 2; pass++) {
                const std::vector<uint16_t> before = d.wind;
                const std::vector<uint16_t> directionBefore = d.direction;
                for (int y = 1; y < d.height - 1; y++) {
                    for (int x = 1; x < d.width - 1; x++) {
                        const size_t at = static_cast<size_t>(y) * static_cast<size_t>(d.width) + static_cast<size_t>(x);
                        if (before[at] != noValue) {
                            continue;
                        }
                        int n = 0, sum = 0;
                        uint16_t anyDirection = noValue;
                        for (int dy = -1; dy <= 1; dy++) {
                            for (int dx = -1; dx <= 1; dx++) {
                                const size_t other = static_cast<size_t>(y + dy) * static_cast<size_t>(d.width) + static_cast<size_t>(x + dx);
                                if (before[other] != noValue) {
                                    n++;
                                    sum += before[other];
                                    if (directionBefore[other] != noValue) {
                                        anyDirection = directionBefore[other];
                                    }
                                }
                            }
                        }
                        if (n >= (pass == 0 ? 4 : 3)) {
                            d.wind[at] = static_cast<uint16_t>(sum / n);
                            d.direction[at] = anyDirection;
                        }
                    }
                }
            }
            d.west = minLon;
            d.east = minLon + d.width * d.stepRad * 180.0 / std::numbers::pi;
            d.north = maxLat;
            d.south = 2.0 * std::atan(std::exp(d.yTop - d.height * d.stepRad)) * 180.0 / std::numbers::pi - 90.0;
            d.maxKnots = top;
            const auto corner = [&] (int row, int column) {
                const size_t i = static_cast<size_t>(row) * static_cast<size_t>(columns) + static_cast<size_t>(column);
                double lo = lon[i];
                if (lo > 180.0) lo -= 360.0;
                return QPointF{lo, static_cast<double>(lat[i])};
            };
            d.outline << corner(0, 0) << corner(0, columns - 1) << corner(rows - 1, columns - 1) << corner(rows - 1, 0);
            d.image = colorize(d.wind, d.width, d.height);
            store.write(name, encodeScene(d));
            *ok = true;
        },
        [this, &host, result, ok, keep, url] {
            if (!entries || keep != entries) {
                return;
            }
            for (auto& e : *entries) {
                if (e.netcdf == url) {
                    e.rendering = false;
                    e.failed = !*ok;
                    if (*ok) {
                        e.data = result;
                    }
                }
            }
            if (*ok) {
                std::lock_guard<std::mutex> lock{memoryMutex};
                memory[url] = result;
                memoryOrder.erase(std::remove(memoryOrder.begin(), memoryOrder.end(), url), memoryOrder.end());
                memoryOrder.push_back(url);
                while (memoryOrder.size() > 12) {
                    memory.erase(memoryOrder.front());
                    memoryOrder.pop_front();
                }
            }
            host.redraw();
        });
}

// wind barbs on the world grid of every other map (BarbGrid): the speed is the SAR's, averaged over the neighborhood of the barb (the radar's 500 m cells are far noisier than a barb is
// meant to show), the direction the one the retrieval used (the model's, in the scene's file)
void SarWindsLayer::paintBarbs(QPainter& painter, MapHost& host, const SceneData& d) const {
    auto& view = host.view();
    const auto t = view.transform();
    const double px = view.unitsPerPixel();
    const double scale = std::max(0.1, host.markScale());
    const double perDegree = std::abs(t(0.0, 1.0).x() - t(0.0, 0.0).x()) / std::max(1e-9, px);
    if (perDegree <= 0.0) {
        return;
    }
    const auto look = BarbGrid::look(view.pixelWidth() / perDegree / scale, perDegree, scale);
    const double cellDegrees = d.stepRad * 180.0 / std::numbers::pi;
    const int half = std::clamp(static_cast<int>(std::lround(look.step * 0.25 / cellDegrees)), 1, 14);   // a barb averages the cells within a quarter of the barb spacing
    const double length = look.length * px;
    painter.save();
    painter.setPen(QPen{QColor{15, 15, 25, 235}, std::max(px, 1.0 * scale * px)});
    painter.setBrush(QColor{15, 15, 25});
    for (double lat = std::ceil(d.south / look.step) * look.step; lat <= d.north; lat += look.step) {
        for (double lon = std::ceil(d.west / look.step) * look.step; lon <= d.east; lon += look.step) {
            const int cx = static_cast<int>((lon - d.west) / cellDegrees), cy = static_cast<int>((d.yTop - mercatorY(lat)) / d.stepRad);
            if (cx < half || cy < half || cx >= d.width - half || cy >= d.height - half) {
                continue;
            }
            double sum = 0.0, sx = 0.0, sy = 0.0;
            int n = 0, nd = 0;
            for (int y = cy - half; y <= cy + half; y++) {
                for (int x = cx - half; x <= cx + half; x++) {
                    const size_t at = static_cast<size_t>(y) * static_cast<size_t>(d.width) + static_cast<size_t>(x);
                    if (d.wind[at] != noValue) {
                        sum += d.wind[at] / 10.0;
                        n++;
                    }
                    if (d.direction[at] != noValue) {
                        const double a = d.direction[at] / 10.0 * std::numbers::pi / 180.0;
                        sx += std::sin(a);
                        sy += std::cos(a);
                        nd++;
                    }
                }
            }
            if (n < (2 * half + 1) * (2 * half + 1) / 3 || nd == 0) {   // most of the neighborhood must have a wind
                continue;
            }
            const double from = std::fmod(std::atan2(sx, sy) * 180.0 / std::numbers::pi + 360.0, 360.0);
            if (host.deferMarks()) {   // a projected world picture: painted upright there, the staff turned to the local north
                const double speed = sum / n, nominal = look.length / scale, south = lat < 0.0;
                host.deferMark(lat, lon, [from, speed, nominal, south] (QPainter& p, double unit, double north) {
                    p.save();
                    p.rotate(north);
                    p.setPen(QPen{QColor{15, 15, 25, 235}, std::max(1.0, unit)});
                    p.setBrush(QColor{15, 15, 25});
                    WindBarb::draw(p, QPointF{0.0, 0.0}, from, speed, nominal * unit, south);
                    p.restore();
                });
                continue;
            }
            WindBarb::draw(painter, t(lat, lon), from, sum / n, length, lat < 0.0);
        }
    }
    painter.restore();
}

void SarWindsLayer::paint(QPainter& painter, MapHost& host) {
    if (!entries) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double px = view.unitsPerPixel();
    const double perDegree = std::abs(t(0.0, 1.0).x() - t(0.0, 0.0).x()) / std::max(1e-9, px);   // screen pixels for a degree of longitude
    const bool close = perDegree > 12.0;   // about 70 degrees across the screen or less: the wind fields are worth reading
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(11 * px));
    painter.setFont(font);
    int started = 0;
    for (size_t i = 0; i < entries->size(); i++) {
        auto& e = (*entries)[i];
        const QPointF at = t(e.scene.lat, e.scene.lon);
        const QRectF window{-500.0, -250.0, 1000.0, 1000.0 * view.pixelHeight() / std::max(1, view.pixelWidth())};
        const bool visible = window.adjusted(-200, -200, 200, 200).contains(at);
        if (close && visible && !e.data && !e.rendering && !e.failed && started < 2) {
            started++;
            render(host, i);
        }
        if (e.data) {
            const auto& d = *e.data;
            const QPointF a = t(d.north, d.west), b = t(d.south, d.east);
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(QRectF{a, b}.normalized(), d.image);
            painter.restore();
            QPolygonF outline;
            for (const auto& p : d.outline) {
                outline << t(p.y(), p.x());
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{255, 255, 255, 190}, 1.2 * px, Qt::DashLine});
            painter.drawPolygon(outline);
            if (showBarbs) {
                paintBarbs(painter, host, d);
            }
        }
        // the mark: a diamond at the middle of the scene with when and by which satellite
        QString label;
        if (perDegree > 4.0) {
            auto sensor = QString::fromStdString(e.scene.sensor);
            sensor = sensor.left(sensor.indexOf(" - ") < 0 ? sensor.size() : sensor.indexOf(" - "));
            label = sensor + " " + e.when.toString("MMM d HH:mm") + "Z" + (!e.data && e.rendering ? " ..." : QString{});
        }
        const QColor fill = e.data ? QColor{255, 255, 255, 220} : QColor{190, 90, 220};
        const auto mark = [fill, label, font] (QPainter& p, double unit, double) {
            QFont f{font};
            f.setPixelSize(std::max(6, static_cast<int>(std::lround(11 * unit))));
            p.setFont(f);
            p.setPen(QPen{QColor{20, 20, 30, 230}, 1.0 * unit});
            p.setBrush(fill);
            const double r = 6.0 * unit;
            p.drawPolygon(QPolygonF{{QPointF{0, -r}, QPointF{r, 0}, QPointF{0, r}, QPointF{-r, 0}}});
            if (!label.isEmpty()) {
                p.setPen(QColor{235, 235, 245});
                p.drawText(QPointF{9.0 * unit, -6.0 * unit}, label);
            }
        };
        if (host.deferMarks()) {
            host.deferMark(e.scene.lat, e.scene.lon, mark);
        } else {
            painter.save();
            painter.translate(at);
            mark(painter, px, 0.0);
            painter.restore();
        }
    }
}

MapHit SarWindsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 16.0;
    hit.priority = 20;
    if (!entries) {
        return hit;
    }
    auto& view = host.view();
    for (const auto& e : *entries) {
        const auto at = view.toPixels(e.scene.lat, e.scene.lon);
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = QString::fromStdString(e.stormId + " " + e.stormName + "\nSAR " + e.scene.sensor + "  " + e.scene.time) +
                       (e.data && e.data->maxKnots > 0 ? QString{"\nstrongest wind in the scene: %1 kt"}.arg(static_cast<int>(std::lround(e.data->maxKnots))) : QString{"\n(zoom in for the wind field)"}) + "\n(click for the storm's SAR winds)";
            const string id = e.stormId, name = e.stormName;
            const int year = e.year;
            hit.open = [id, name, year] (Window * parent) { new SarWindsViewer{parent, id, name, year}; };
        }
    }
    return hit;
}

QWidget * SarWindsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * check = new QCheckBox{"Wind barbs (the SAR's speed, the direction of the model wind the retrieval used)", widget};
    check->setChecked(showBarbs);
    QObject::connect(check, &QCheckBox::toggled, [this, changed] (bool on) { showBarbs = on; changed(); });
    layout->addWidget(check);
    return widget;
}

vector<MapLegendRow> SarWindsLayer::legend() const {
    MapLegendRow row;
    row.title = "SAR wind, 10 m (kt):";
    for (const double kt : {0.0, 15.0, 34.0, 50.0, 64.0, 83.0, 96.0, 113.0, 137.0}) {
        const QRgb c = windColor(kt);
        row.entries.push_back({MapLegendEntry::Square, QColor{qRed(c), qGreen(c), qBlue(c)}, QString::number(static_cast<int>(kt))});
    }
    return {row};
}
