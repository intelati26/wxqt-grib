// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GibsLayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QComboBox>
#include <QTimeZone>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QDateTime>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>
#include "radar/MapView.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

const std::vector<GibsLayer::Product>& GibsLayer::products() {
    static const std::vector<Product> all{
        {"VIIRS_NOAA20_CorrectedReflectance_TrueColor", "NOAA-20 VIIRS true color", "jpg", "GoogleMapsCompatible_Level9", 9},
        {"VIIRS_NOAA21_CorrectedReflectance_TrueColor", "NOAA-21 VIIRS true color", "jpg", "GoogleMapsCompatible_Level9", 9},
        {"VIIRS_SNPP_CorrectedReflectance_TrueColor", "Suomi NPP VIIRS true color", "jpg", "GoogleMapsCompatible_Level9", 9},
        {"MODIS_Terra_CorrectedReflectance_TrueColor", "Terra MODIS true color", "jpg", "GoogleMapsCompatible_Level9", 9},
        {"MODIS_Aqua_CorrectedReflectance_TrueColor", "Aqua MODIS true color", "jpg", "GoogleMapsCompatible_Level9", 9},
        {"VIIRS_NOAA20_CorrectedReflectance_BandsM11-I2-I1", "NOAA-20 false color (M11-I2-I1: snow and ice cyan, fire scars)", "jpg", "GoogleMapsCompatible_Level9", 9},
        {"VIIRS_NOAA20_Brightness_Temp_BandI5_Day", "NOAA-20 infrared window (day side)", "png", "GoogleMapsCompatible_Level9", 9},
        {"VIIRS_NOAA20_Brightness_Temp_BandI5_Night", "NOAA-20 infrared window (night side)", "png", "GoogleMapsCompatible_Level9", 9},
        {"VIIRS_NOAA20_DayNightBand_At_Sensor_Radiance", "NOAA-20 day / night band (night lights, moonlit cloud)", "png", "GoogleMapsCompatible_Level8", 8},
    };
    return all;
}

GibsLayer::GibsLayer() {
    productIndex = std::clamp(Utility::readPrefInt("MM_GIBS_PRODUCT", 0), 0, static_cast<int>(products().size()) - 1);
    daysBack = std::clamp(Utility::readPrefInt("MM_GIBS_DAYS", 1), 0, 4000);
    liveDays = daysBack;
}

string GibsLayer::dateText() const {
    return QDateTime::currentDateTimeUtc().addDays(-daysBack).toString("yyyy-MM-dd").toStdString();
}

string GibsLayer::summary() const {
    return string{products()[static_cast<size_t>(productIndex)].label} + "  " + dateText() + (daysBack == 0 ? "  (today so far)" : "");
}

namespace {
    double tileLon(double x, int z) { return x / std::pow(2.0, z) * 360.0 - 180.0; }
    double tileLat(double y, int z) {
        const double n = std::numbers::pi * (1.0 - 2.0 * y / std::pow(2.0, z));
        return std::atan(std::sinh(n)) * 180.0 / std::numbers::pi;
    }
}

string GibsLayer::tileKey(const Product& product, const string& date, const Tile& tile) {
    return string{product.id} + "|" + date + "|" + std::to_string(tile.z) + "/" + std::to_string(tile.y) + "/" + std::to_string(tile.x);
}

string GibsLayer::urlOf(const Product& product, const string& date, const Tile& tile) const {
    return string{"https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/"} + product.id + "/default/" + date + "/" + product.matrixSet + "/" + std::to_string(tile.z) + "/" +
        std::to_string(tile.y) + "/" + std::to_string(tile.x) + "." + product.extension;
}

namespace {
    // the disk copies of complete days (a day that is still being made is not kept): one file a tile, dropped after a month or when the folder passes a size
    QString cacheFile(const std::string& key, const char * extension) {
        const auto folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/gibs";
        QDir{}.mkpath(folder);
        const auto hash = QCryptographicHash::hash(QByteArray::fromStdString(key), QCryptographicHash::Sha1).toHex();
        return folder + "/" + QString::fromLatin1(hash) + "." + extension;
    }

    void pruneCache() {
        static bool done = false;
        if (done) {
            return;
        }
        done = true;
        const QDir dir{QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/gibs"};
        auto files = dir.entryInfoList(QDir::Files, QDir::Time);   // newest first
        const auto cutoff = QDateTime::currentDateTime().addDays(-30);
        qint64 total = 0;
        for (const auto& f : files) {
            total += f.size();
        }
        for (int i = static_cast<int>(files.size()) - 1; i >= 0; i--) {   // the oldest first
            if (files[static_cast<qsizetype>(i)].lastModified() < cutoff || total > 600LL * 1024 * 1024) {
                total -= files[static_cast<qsizetype>(i)].size();
                QFile::remove(files[static_cast<qsizetype>(i)].filePath());
            }
        }
    }
}

// the tiles that cover the view, at the zoom whose tile (256 pixels) is about the size of the pixels it covers; coarser when that would be a great many tiles
std::vector<GibsLayer::Tile> GibsLayer::tilesInView(MapHost& host, int& zoom) const {
    auto& view = host.view();
    const auto& product = products()[static_cast<size_t>(productIndex)];
    const auto topLeft = view.toLatLon(QPointF{0.0, 0.0});
    const auto bottomRight = view.toLatLon(QPointF{static_cast<double>(view.pixelWidth()), static_cast<double>(view.pixelHeight())});
    const double north = std::min(topLeft.first, 85.0), south = std::max(bottomRight.first, -85.0);
    double west = topLeft.second, east = bottomRight.second;
    if (east - west < 1.0e-6 || east - west > 360.0) {
        west = -180.0;
        east = 180.0;
    }
    const double pixelsPerDegree = view.pixelWidth() / std::max(1.0e-6, east - west);
    int z = std::clamp(static_cast<int>(std::lround(std::log2(pixelsPerDegree * 360.0 / 256.0))), 1, product.maxZoom);
    const auto range = [&] (int level, int& x0, int& x1, int& y0, int& y1) {
        const double n = std::pow(2.0, level);
        x0 = std::max(0, static_cast<int>(std::floor((west + 180.0) / 360.0 * n)));
        x1 = std::min(static_cast<int>(n) - 1, static_cast<int>(std::floor((east + 180.0) / 360.0 * n)));
        const auto row = [n] (double lat) {
            const double r = lat * std::numbers::pi / 180.0;
            return static_cast<int>(std::floor((1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / std::numbers::pi) / 2.0 * n));
        };
        y0 = std::max(0, row(north));
        y1 = std::min(static_cast<int>(n) - 1, row(south));
    };
    int x0, x1, y0, y1;
    range(z, x0, x1, y0, y1);
    while (z > 1 && (x1 - x0 + 1) * (y1 - y0 + 1) > 48) {
        z--;
        range(z, x0, x1, y0, y1);
    }
    zoom = z;
    std::vector<Tile> out;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            out.push_back({z, x, y});
        }
    }
    return out;
}

// one tile: the disk copy if it is there, else the network (and kept on disk when the day is complete); asked for once, the map painted again as it arrives
void GibsLayer::request(MapHost& host, const Product& product, const string& date, const Tile& tile) {
    const string key = tileKey(product, date, tile);
    if (tiles.count(key) > 0 || asked.count(key) > 0) {
        return;
    }
    asked.insert(key);
    const string url = urlOf(product, date, tile);
    const bool keep = date < QDateTime::currentDateTimeUtc().addDays(-1).toString("yyyy-MM-dd").toStdString();
    const QString file = cacheFile(key, product.extension);
    auto bytes = std::make_shared<QByteArray>();
    host.background([url, bytes, keep, file] {
                        QFile disk{file};
                        if (keep && disk.open(QIODevice::ReadOnly)) {
                            *bytes = disk.readAll();
                            return;
                        }
                        *bytes = UtilityIO::downloadAsByteArray(url);
                        if (keep && !bytes->isEmpty() && !QImage::fromData(*bytes).isNull() && disk.open(QIODevice::WriteOnly)) {
                            disk.write(*bytes);
                        }
                    },
                    [this, key, bytes, &host] {
                        if (!bytes->isEmpty()) {
                            const auto decoded = QImage::fromData(*bytes);
                            if (!decoded.isNull()) {
                                tiles[key] = decoded;
                            }
                        }
                        host.redraw();
                    });
}

void GibsLayer::paint(QPainter& painter, MapHost& host) {
    pruneCache();
    auto& view = host.view();
    const auto& product = products()[static_cast<size_t>(productIndex)];
    const auto t = view.transform();
    int z = 1;
    const auto wanted = tilesInView(host, z);
    shownZoom = z;
    const string date = dateText();
    const double bleed = 0.5 * view.unitsPerPixel();   // a half pixel over each edge: no hairline between tiles
    painter.save();
    painter.setOpacity(opacity / 100.0);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    int started = 0;
    for (const auto& tile : wanted) {
        const string key = tileKey(product, date, tile);
        const auto found = tiles.find(key);
        const QPointF a = t(tileLat(tile.y, tile.z), tileLon(tile.x, tile.z)), b = t(tileLat(tile.y + 1, tile.z), tileLon(tile.x + 1, tile.z));
        const QRectF where = QRectF{a, b}.normalized().adjusted(-bleed, -bleed, bleed, bleed);
        if (found != tiles.end()) {
            painter.drawImage(where, found->second);
            continue;
        }
        if (started < 8 && asked.count(key) == 0) {
            started++;
            request(host, product, date, tile);
        }
        // while a tile is on its way a coarser one that is already here stands in (up to three levels)
        for (int up = 1; up <= 3 && tile.z - up >= 0; up++) {
            const auto parent = tiles.find(tileKey(product, date, {tile.z - up, tile.x >> up, tile.y >> up}));
            if (parent != tiles.end()) {
                const int span = 1 << up;
                const int px = tile.x & (span - 1), py = tile.y & (span - 1);
                const QImage part = parent->second.copy(parent->second.width() * px / span, parent->second.height() * py / span, parent->second.width() / span, parent->second.height() / span);
                painter.drawImage(where, part);
                break;
            }
        }
    }
    painter.restore();
    if (tiles.size() > 600) {   // the session's tiles are kept up to a few hundred, then started over (the disk copies stay)
        tiles.clear();
        asked.clear();
    }
}

// the days as the map's frames: noon UTC of each of the last fifteen days, oldest first (today is the partial day)
vector<long> GibsLayer::times() const {
    vector<long> out;
    const auto today = QDateTime::currentDateTimeUtc().date();
    for (int back = 14; back >= 0; back--) {
        out.push_back(static_cast<long>(QDateTime{today.addDays(-back), QTime{12, 0}, QTimeZone::utc()}.toSecsSinceEpoch()));
    }
    return out;
}

string GibsLayer::timeText() const {
    return string{products()[static_cast<size_t>(productIndex)].label} + "  " + dateText();
}

void GibsLayer::showTime(long seconds, MapHost& host) {
    if (seconds == 0) {
        daysBack = liveDays;   // live: the day chosen in the options
    } else {
        const auto day = QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).date();
        daysBack = std::clamp(static_cast<int>(day.daysTo(QDateTime::currentDateTimeUtc().date())), 0, 4000);
    }
    host.redraw();
}

// a loop through the days: the tiles of the view for each of them, read ahead (from disk when kept), a few at a time as the map is painted
void GibsLayer::prepareTimes(MapHost& host) {
    const auto& product = products()[static_cast<size_t>(productIndex)];
    int z = 1;
    const auto wanted = tilesInView(host, z);
    const auto today = QDateTime::currentDateTimeUtc().date();
    int started = 0;
    for (int back = 14; back >= 0 && started < 160; back--) {
        const string date = today.addDays(-back).toString("yyyy-MM-dd").toStdString();
        for (const auto& tile : wanted) {
            if (tiles.count(tileKey(product, date, tile)) == 0 && asked.count(tileKey(product, date, tile)) == 0) {
                request(host, product, date, tile);
                started++;
            }
        }
    }
    host.timesChanged();
}

QWidget * GibsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Imagery:", widget});
    auto * combo = new QComboBox{widget};
    for (const auto& p : products()) {
        combo->addItem(p.label);
    }
    combo->setCurrentIndex(productIndex);
    layout->addWidget(combo);
    layout->addWidget(new QLabel{"Day (UTC):", widget});
    auto * day = new QComboBox{widget};
    day->addItem("Today so far");
    day->addItem("Yesterday");
    for (int d = 2; d <= 14; d++) {
        day->addItem(QDateTime::currentDateTimeUtc().addDays(-d).toString("ddd yyyy-MM-dd"));
    }
    day->setCurrentIndex(std::clamp(daysBack, 0, day->count() - 1));
    layout->addWidget(day);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        productIndex = index;
        Utility::writePrefInt("MM_GIBS_PRODUCT", index);
        changed();
    });
    QObject::connect(day, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        daysBack = index;
        liveDays = index;
        Utility::writePrefInt("MM_GIBS_DAYS", index);
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}

void GibsLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    daysBack = std::clamp(static_cast<int>(utc.toUTC().date().daysTo(QDateTime::currentDateTimeUtc().date())), 0, 4000);
    liveDays = daysBack;
    host.redraw();
}
