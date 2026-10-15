// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GibsLayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QComboBox>
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
    daysBack = std::clamp(Utility::readPrefInt("MM_GIBS_DAYS", 1), 0, 60);
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

void GibsLayer::paint(QPainter& painter, MapHost& host) {
    auto& view = host.view();
    const auto& product = products()[static_cast<size_t>(productIndex)];
    const auto t = view.transform();
    const auto * map = view.map();
    // the part of the world in view, from the corners of the widget
    const auto topLeft = view.toLatLon(QPointF{0.0, 0.0});
    const auto bottomRight = view.toLatLon(QPointF{static_cast<double>(map->width()), static_cast<double>(map->height())});
    const double north = std::min(topLeft.first, 85.0), south = std::max(bottomRight.first, -85.0);
    double west = topLeft.second, east = bottomRight.second;
    if (east - west < 1.0e-6 || east - west > 360.0) {
        west = -180.0;
        east = 180.0;
    }
    // the zoom whose tile (256 pixels) is about the size of the pixels it covers; coarser when that would be a great many tiles
    const double pixelsPerDegree = map->width() / std::max(1.0e-6, east - west);
    int z = std::clamp(static_cast<int>(std::lround(std::log2(pixelsPerDegree * 360.0 / 256.0))), 1, product.maxZoom);
    const auto range = [&] (int zoom, int& x0, int& x1, int& y0, int& y1) {
        const double n = std::pow(2.0, zoom);
        x0 = static_cast<int>(std::floor((west + 180.0) / 360.0 * n));
        x1 = static_cast<int>(std::floor((east + 180.0) / 360.0 * n));
        const auto row = [n] (double lat) {
            const double r = lat * std::numbers::pi / 180.0;
            return static_cast<int>(std::floor((1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / std::numbers::pi) / 2.0 * n));
        };
        y0 = std::max(0, row(north));
        y1 = std::min(static_cast<int>(n) - 1, row(south));
        x0 = std::max(0, x0);
        x1 = std::min(static_cast<int>(n) - 1, x1);
    };
    int x0, x1, y0, y1;
    range(z, x0, x1, y0, y1);
    while (z > 1 && (x1 - x0 + 1) * (y1 - y0 + 1) > 48) {
        z--;
        range(z, x0, x1, y0, y1);
    }
    shownZoom = z;
    const string date = dateText();
    const string base = string{"https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/"} + product.id + "/default/" + date + "/" + product.matrixSet + "/";
    const auto keyOf = [&] (int zoom, int x, int y) { return string{product.id} + "|" + date + "|" + std::to_string(zoom) + "/" + std::to_string(y) + "/" + std::to_string(x); };
    painter.save();
    painter.setOpacity(opacity / 100.0);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    int started = 0;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const string key = keyOf(z, x, y);
            const auto found = tiles.find(key);
            QImage * image = found == tiles.end() ? nullptr : &found->second;
            if (image == nullptr && asked.count(key) == 0 && started < 8) {   // asked for once, a few at a time; the picture is painted again as each arrives
                asked.insert(key);
                started++;
                const string url = base + std::to_string(z) + "/" + std::to_string(y) + "/" + std::to_string(x) + "." + product.extension;
                auto bytes = std::make_shared<QByteArray>();
                host.background([url, bytes] { *bytes = UtilityIO::downloadAsByteArray(url); },
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
            if (image == nullptr) {
                // while a tile is on its way a coarser one that is already here stands in (up to three levels)
                for (int up = 1; up <= 3 && image == nullptr && z - up >= 0; up++) {
                    const auto parent = tiles.find(keyOf(z - up, x >> up, y >> up));
                    if (parent != tiles.end()) {
                        const int span = 1 << up;
                        const int px = (x & (span - 1)), py = (y & (span - 1));
                        const QImage part = parent->second.copy(parent->second.width() * px / span, parent->second.height() * py / span, parent->second.width() / span, parent->second.height() / span);
                        const QPointF a = t(tileLat(y, z), tileLon(x, z)), b = t(tileLat(y + 1, z), tileLon(x + 1, z));
                        painter.drawImage(QRectF{a, b}, part);
                        break;
                    }
                }
                continue;
            }
            const QPointF a = t(tileLat(y, z), tileLon(x, z)), b = t(tileLat(y + 1, z), tileLon(x + 1, z));
            painter.drawImage(QRectF{a, b}, *image);
        }
    }
    painter.restore();
    if (tiles.size() > 600) {   // the session's tiles are kept up to a few hundred, then started over
        tiles.clear();
        asked.clear();
    }
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
        Utility::writePrefInt("MM_GIBS_DAYS", index);
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
