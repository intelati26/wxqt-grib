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
#include <QDateTime>
#include <QtEndian>
#include <cstring>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include "hurricane/SarWindsViewer.h"
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

    // The scenes already painted, kept until the program is closed (not on disk: a scene is a 16 MB download): every map window shares them; the 12 newest used stay
    struct Painted {
        QImage image;
        double west{0}, south{0}, east{0}, north{0}, maxKnots{0};
        QPolygonF outline;
    };
    std::mutex paintedMutex;
    std::map<string, Painted> painted;
    std::deque<string> paintedOrder;   // oldest first

    bool paintedFor(const string& url, Painted& out) {
        std::lock_guard<std::mutex> lock{paintedMutex};
        const auto found = painted.find(url);
        if (found == painted.end()) {
            return false;
        }
        out = found->second;
        paintedOrder.erase(std::remove(paintedOrder.begin(), paintedOrder.end(), url), paintedOrder.end());
        paintedOrder.push_back(url);
        return true;
    }

    void keepPainted(const string& url, Painted scene) {
        std::lock_guard<std::mutex> lock{paintedMutex};
        painted[url] = std::move(scene);
        paintedOrder.erase(std::remove(paintedOrder.begin(), paintedOrder.end(), url), paintedOrder.end());
        paintedOrder.push_back(url);
        while (paintedOrder.size() > 12) {
            painted.erase(paintedOrder.front());
            paintedOrder.pop_front();
        }
    }

    constexpr uint16_t noWind = 65535;

    // the picture of a grid of winds (tenths of a knot): the colors, the cells the rotated grid missed filled from their neighbors
    QImage colorize(const std::vector<uint16_t>& grid, int width, int height) {
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const uint16_t v = grid[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
                if (v != noWind) {
                    image.setPixel(x, y, windColor(v / 10.0));
                }
            }
        }
        return image;
    }

    // what is kept on disk of a scene, whole: the grid and where it is (about a megabyte compressed; the 16 MB file is not)
    string encodeScene(const std::vector<uint16_t>& grid, int width, int height, const double bounds[5], const QPolygonF& outline) {
        string out{"SAR1"};
        const auto put = [&out] (const void * data, size_t n) { out.append(static_cast<const char *>(data), n); };
        put(&width, 4);
        put(&height, 4);
        put(bounds, 5 * sizeof(double));
        const int corners = static_cast<int>(outline.size());
        put(&corners, 4);
        for (const auto& p : outline) {
            const double xy[2] = {p.x(), p.y()};
            put(xy, sizeof(xy));
        }
        put(grid.data(), grid.size() * 2);
        return out;
    }

    bool decodeScene(const string& in, std::vector<uint16_t>& grid, int& width, int& height, double bounds[5], QPolygonF& outline) {
        if (in.size() < 4 + 8 + 5 * sizeof(double) + 4 || in.compare(0, 4, "SAR1") != 0) {
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
        int corners = 0;
        if (!get(&width, 4) || !get(&height, 4) || !get(bounds, 5 * sizeof(double)) || !get(&corners, 4) || width <= 0 || height <= 0 || width > 4000 || height > 4000 || corners < 0 || corners > 16) {
            return false;
        }
        outline.clear();
        for (int i = 0; i < corners; i++) {
            double xy[2];
            if (!get(xy, sizeof(xy))) {
                return false;
            }
            outline << QPointF{xy[0], xy[1]};
        }
        grid.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
        return get(grid.data(), grid.size() * 2);
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
}

string SarWindsLayer::summary() const {
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
            // the scenes already painted are kept when the list is read again
            if (entries) {
                for (auto& e : *fresh) {
                    for (auto& old : *entries) {
                        if (old.netcdf == e.netcdf && !old.image.isNull()) {
                            e = old;
                        }
                    }
                }
            }
            entries = fresh;
            error = fresh->empty() ? *message : string{};
            host.redraw();
        });
}

// the scene's wind field: STAR's NetCDF, its wind (m/s), latitude and longitude read with GDAL, painted into a picture on a regular grid in Mercator
void SarWindsLayer::render(MapHost& host, size_t index) {
    auto& entry = (*entries)[index];
    if (entry.rendering || !entry.image.isNull() || entry.failed) {
        return;
    }
    const auto url = entry.netcdf;
    if (Painted again; paintedFor(url, again)) {   // painted before in this session (here or in another window): no download
        entry.image = again.image;
        entry.west = again.west;
        entry.east = again.east;
        entry.north = again.north;
        entry.south = again.south;
        entry.outline = again.outline;
        entry.maxKnots = again.maxKnots;
        host.redraw();
        return;
    }
    entry.rendering = true;
    auto result = std::make_shared<Entry>();
    auto keep = entries;
    host.background(
        [result, url] {
            // a scene read before, in an earlier session: its grid is on disk (a scene never changes once STAR has published it)
            const PermanentCache store{"sar"};
            const string name = url.substr(url.rfind('/') + 1) + ".bin";
            {
                std::vector<uint16_t> grid;
                int w = 0, h = 0;
                double bounds[5];
                QPolygonF outline;
                if (const auto text = store.read(name); !text.empty() && decodeScene(text, grid, w, h, bounds, outline)) {
                    result->image = colorize(grid, w, h);
                    result->west = bounds[0];
                    result->south = bounds[1];
                    result->east = bounds[2];
                    result->north = bounds[3];
                    result->maxKnots = bounds[4];
                    result->outline = outline;
                    return;
                }
            }
            if (UtilityGrib::gdalBinDir().empty()) {
                result->failed = true;
                return;
            }
            const auto bytes = URL::getBytes(url);
            if (bytes.size() < 10000) {
                result->failed = true;
                return;
            }
            QTemporaryDir dir;
            if (!dir.isValid()) {
                result->failed = true;
                return;
            }
            const QString file = dir.filePath("scene.nc");
            {
                QFile f{file};
                if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) {
                    result->failed = true;
                    return;
                }
            }
            int c1 = 0, r1 = 0, c2 = 0, r2 = 0, c3 = 0, r3 = 0;
            std::vector<float> wind, lat, lon;
            if (!readLayer(file, "sar_wind", dir.filePath("w.raw"), c1, r1, wind) || !readLayer(file, "latitude", dir.filePath("a.raw"), c2, r2, lat) || !readLayer(file, "longitude", dir.filePath("o.raw"), c3, r3, lon) ||
                c1 != c2 || c1 != c3 || r1 != r2 || r1 != r3) {
                result->failed = true;
                return;
            }
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
                result->failed = true;
                return;
            }
            // a picture with one pixel for about the size of a cell of the scene (a square in Mercator)
            const double step = std::max(0.004, (maxLon - minLon) / 1400.0) * std::numbers::pi / 180.0;   // radians of longitude per pixel
            const int width = std::clamp(static_cast<int>(std::ceil((maxLon - minLon) * std::numbers::pi / 180.0 / step)) + 2, 8, 1600);
            const double yTop = mercatorY(maxLat);
            const int height = std::clamp(static_cast<int>(std::ceil((yTop - mercatorY(minLat)) / step)) + 2, 8, 1600);
            std::vector<uint16_t> grid(static_cast<size_t>(width) * static_cast<size_t>(height), noWind);
            double top = 0.0;
            for (int row = 0; row < r1; row++) {
                for (int column = 0; column < c1; column++) {
                    const size_t i = static_cast<size_t>(row) * static_cast<size_t>(c1) + static_cast<size_t>(column);
                    const float w = wind[i];
                    if (std::isnan(w) || w < 0.0f || std::isnan(lat[i]) || std::abs(lat[i]) > 90.0) {
                        continue;
                    }
                    double lo = lon[i];
                    if (lo > 180.0) lo -= 360.0;
                    const int x = static_cast<int>((lo - minLon) * std::numbers::pi / 180.0 / step);
                    const int y = static_cast<int>((yTop - mercatorY(lat[i])) / step);
                    if (x >= 0 && y >= 0 && x < width && y < height) {
                        grid[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = static_cast<uint16_t>(std::clamp(w * toKnots * 10.0, 0.0, 60000.0));
                        top = std::max(top, static_cast<double>(w * toKnots));
                    }
                }
            }
            // the holes inside the scene (cells the rotated grid missed): the average of the neighbors that have a wind
            std::vector<uint16_t> filled = grid;
            for (int y = 1; y < height - 1; y++) {
                for (int x = 1; x < width - 1; x++) {
                    if (grid[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] != noWind) {
                        continue;
                    }
                    int n = 0, sum = 0;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            const uint16_t v = grid[static_cast<size_t>(y + dy) * static_cast<size_t>(width) + static_cast<size_t>(x + dx)];
                            if (v != noWind) {
                                n++;
                                sum += v;
                            }
                        }
                    }
                    if (n >= 5) {
                        filled[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = static_cast<uint16_t>(sum / n);
                    }
                }
            }
            result->image = colorize(filled, width, height);
            result->west = minLon;
            result->east = minLon + width * step * 180.0 / std::numbers::pi;
            result->north = maxLat;
            result->south = 2.0 * std::atan(std::exp(yTop - height * step)) * 180.0 / std::numbers::pi - 90.0;
            result->maxKnots = top;
            // the corners of the scene
            const auto corner = [&] (int row, int column) {
                const size_t i = static_cast<size_t>(row) * static_cast<size_t>(c1) + static_cast<size_t>(column);
                double lo = lon[i];
                if (lo > 180.0) lo -= 360.0;
                return QPointF{lo, static_cast<double>(lat[i])};
            };
            result->outline << corner(0, 0) << corner(0, c1 - 1) << corner(r1 - 1, c1 - 1) << corner(r1 - 1, 0);
            const double bounds[5] = {result->west, result->south, result->east, result->north, result->maxKnots};
            store.write(name, encodeScene(filled, width, height, bounds, result->outline));
        },
        [this, &host, result, keep, url] {
            if (!entries || keep != entries) {
                return;
            }
            for (auto& e : *entries) {
                if (e.netcdf == url) {
                    e.rendering = false;
                    e.failed = result->failed;
                    if (!result->failed) {
                        keepPainted(url, Painted{result->image, result->west, result->south, result->east, result->north, result->maxKnots, result->outline});
                        e.image = result->image;
                        e.west = result->west;
                        e.east = result->east;
                        e.north = result->north;
                        e.south = result->south;
                        e.outline = result->outline;
                        e.maxKnots = result->maxKnots;
                    }
                }
            }
            host.redraw();
        });
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
        if (close && visible && e.image.isNull() && !e.rendering && !e.failed && started < 2) {
            started++;
            render(host, i);
        }
        if (!e.image.isNull()) {
            const QPointF a = t(e.north, e.west), b = t(e.south, e.east);
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.drawImage(QRectF{a, b}.normalized(), e.image);
            painter.restore();
            QPolygonF outline;
            for (const auto& p : e.outline) {
                outline << t(p.y(), p.x());
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{255, 255, 255, 190}, 1.2 * px, Qt::DashLine});
            painter.drawPolygon(outline);
        }
        // the mark: a diamond at the middle of the scene with when and by which satellite
        painter.setPen(QPen{QColor{20, 20, 30, 230}, 1.0 * px});
        painter.setBrush(e.image.isNull() ? QColor{190, 90, 220} : QColor{255, 255, 255, 220});
        const double r = 6.0 * px;
        painter.drawPolygon(QPolygonF{{at + QPointF{0, -r}, at + QPointF{r, 0}, at + QPointF{0, r}, at + QPointF{-r, 0}}});
        if (perDegree > 4.0) {
            auto sensor = QString::fromStdString(e.scene.sensor);
            sensor = sensor.left(sensor.indexOf(" - ") < 0 ? sensor.size() : sensor.indexOf(" - "));
            painter.setPen(QColor{235, 235, 245});
            painter.drawText(at + QPointF{9.0 * px, -6.0 * px}, sensor + " " + e.when.toString("MMM d HH:mm") + "Z" + (e.image.isNull() && e.rendering ? " ..." : QString{}));
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
            hit.text = QString::fromStdString(e.stormId + " " + e.stormName + "\nSAR " + e.scene.sensor + "  " + e.scene.time) + (e.maxKnots > 0 ? QString{"\nstrongest wind in the scene: %1 kt"}.arg(static_cast<int>(std::lround(e.maxKnots))) : QString{"\n(zoom in for the wind field)"}) +
                       "\n(click for the storm's SAR winds)";
            const string id = e.stormId, name = e.stormName;
            const int year = e.year;
            hit.open = [id, name, year] (Window * parent) { new SarWindsViewer{parent, id, name, year}; };
        }
    }
    return hit;
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
