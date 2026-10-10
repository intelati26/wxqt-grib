// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GoesLayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include "util/UtilityIO.h"

const GoesLayer::Satellite& GoesLayer::satellite(int index) {
    static const Satellite all[] = {
        {"GOES-East (GOES-19)", "GOES19", -75.2},
        {"GOES-West (GOES-18)", "GOES18", -137.0},
    };
    return all[std::clamp(index, 0, 1)];
}

string GoesLayer::url() const {
    return std::string{"https://cdn.star.nesdis.noaa.gov/"} + satellite(satelliteIndex).folder + "/ABI/FD/" + (product == 0 ? "Sandwich" : "GEOCOLOR") + "/5424x5424.jpg";
}

string GoesLayer::summary() const {
    if (loading && !picture) return "reading the GOES picture (about 17 MB)...";
    if (!error.empty()) return error;
    if (!picture) return {};
    return std::string{satellite(satelliteIndex).name} + (product == 0 ? " Sandwich" : " GeoColor") + ", read at " + loadedAt + "Z";
}

void GoesLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const int mine = ++generation;
    auto fresh = std::make_shared<QImage>();
    auto message = std::make_shared<string>();
    const auto address = url();
    host.background(
        [fresh, message, address] {
            const auto bytes = UtilityIO::downloadAsByteArray(address);
            if (bytes.size() < 100000 || !fresh->loadFromData(bytes)) {
                *message = "The GOES picture could not be read.";
                *fresh = QImage{};
            }
        },
        [this, &host, fresh, message, mine] {
            loading = false;
            if (mine != generation) {
                return;
            }
            if (fresh->isNull()) {
                error = *message;
            } else {
                error.clear();
                picture = std::make_shared<QImage>(fresh->convertToFormat(QImage::Format_RGB32));
                loadedAt = QDateTime::currentDateTimeUtc().toString("HH:mm").toStdString();
                cacheWidth = 0;   // draw it afresh
            }
            host.redraw();
        });
}

// Latitude and longitude to the scan angles of the ABI fixed grid (GOES-R Product User's Guide, section 4.2.8.2), then to a pixel of the full disk image.
void GoesLayer::paint(QPainter& painter, MapHost& host) {
    if (!picture || picture->isNull()) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double ratio = painter.device()->devicePixelRatioF();
    const int width = std::max(1, static_cast<int>(std::lround(view.map()->width() * ratio)));
    const int height = std::max(1, static_cast<int>(std::lround(view.map()->height() * ratio)));
    const bool same = cacheWidth == width && cacheHeight == height && cacheZoom == t.zoom && cacheX == t.xPos && cacheY == t.yPos && cacheSatellite == satelliteIndex &&
        cacheBrightness == brightness && cachePicture == picture.get();
    if (!same) {
        constexpr double rEq = 6378137.0;
        constexpr double rPol = 6356752.31414;
        constexpr double satelliteHeight = 42164160.0;   // from the centre of the earth
        constexpr double ifov = 2.0 * 0.151844 / 5424.0;   // the angle of one pixel, radians
        const double lon0 = satellite(satelliteIndex).longitude * std::numbers::pi / 180.0;
        const double e2 = (rEq * rEq - rPol * rPol) / (rEq * rEq);
        const double ratio2 = (rPol * rPol) / (rEq * rEq);
        const int side = picture->width();
        const double half = side / 2.0;
        const double factor = brightness / 100.0;
        cache = QImage{width, height, QImage::Format_ARGB32_Premultiplied};
        // the longitude of each column (the map is Mercator: longitude is linear in x)
        std::vector<double> lonOf(static_cast<size_t>(width));
        for (int i = 0; i < width; i++) {
            const double u = (i + 0.5) / ratio * 1000.0 / view.map()->width() - 500.0;
            lonOf[static_cast<size_t>(i)] = (((u - t.xPos) / t.zoom - t.bx) / t.ax) * std::numbers::pi / 180.0;
        }
        for (int j = 0; j < height; j++) {
            const double v = (j + 0.5) / ratio * 1000.0 / view.map()->height() - 250.0;
            const double mercator = ((v - t.yPos) / t.zoom - t.by) / t.ay;
            const double lat = std::atan(std::sinh(mercator * std::numbers::pi / 180.0));
            auto * out = reinterpret_cast<QRgb *>(cache.scanLine(j));
            if (std::fabs(lat) > 1.45) {
                std::fill(out, out + width, qRgba(0, 0, 0, 0));
                continue;
            }
            const double phiC = std::atan(ratio2 * std::tan(lat));
            const double rc = rPol / std::sqrt(1.0 - e2 * std::cos(phiC) * std::cos(phiC));
            const double cosPhi = std::cos(phiC);
            const double sinPhi = std::sin(phiC);
            for (int i = 0; i < width; i++) {
                double dl = lonOf[static_cast<size_t>(i)] - lon0;
                dl = std::remainder(dl, 2.0 * std::numbers::pi);
                const double sx = satelliteHeight - rc * cosPhi * std::cos(dl);
                const double sy = -rc * cosPhi * std::sin(dl);
                const double sz = rc * sinPhi;
                // the point is on the visible side of the earth when it is not hidden by the limb
                if (satelliteHeight * (satelliteHeight - sx) < sy * sy + (rEq * rEq) / (rPol * rPol) * sz * sz) {
                    out[i] = qRgba(0, 0, 0, 0);
                    continue;
                }
                const double x = std::asin(-sy / std::sqrt(sx * sx + sy * sy + sz * sz));
                const double y = std::atan(sz / sx);
                const int column = static_cast<int>(std::floor(x / ifov + half));
                const int row = static_cast<int>(std::floor(half - y / ifov));
                if (column < 0 || column >= side || row < 0 || row >= side) {
                    out[i] = qRgba(0, 0, 0, 0);
                    continue;
                }
                const QRgb c = reinterpret_cast<const QRgb *>(picture->constScanLine(row))[column];
                out[i] = factor == 1.0 ? (c | 0xff000000u) : qRgba(std::min(255, static_cast<int>(qRed(c) * factor)), std::min(255, static_cast<int>(qGreen(c) * factor)), std::min(255, static_cast<int>(qBlue(c) * factor)), 255);
            }
        }
        cacheWidth = width;
        cacheHeight = height;
        cacheZoom = t.zoom;
        cacheX = t.xPos;
        cacheY = t.yPos;
        cacheSatellite = satelliteIndex;
        cacheBrightness = brightness;
        cachePicture = picture.get();
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRectF{-500.0, -250.0, 1000.0, 1000.0 * view.map()->height() / std::max(1, view.map()->width())}, cache, QRectF{cache.rect()});
}

QWidget * GoesLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Satellite:", widget});
    auto * sat = new QComboBox{widget};
    sat->addItems({satellite(0).name, satellite(1).name});
    sat->setCurrentIndex(satelliteIndex);
    layout->addWidget(sat);
    layout->addWidget(new QLabel{"Picture:", widget});
    auto * kind = new QComboBox{widget};
    kind->addItems({"Sandwich (visible with enhanced infrared)", "GeoColor (true colour, infrared at night)"});
    kind->setCurrentIndex(product);
    layout->addWidget(kind);
    layout->addWidget(new QLabel{"Brightness:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(40, 160);
    slider->setValue(brightness);
    layout->addWidget(slider);
    layout->addWidget(new QLabel{"A new picture is read when the layers are refreshed.", widget});
    QObject::connect(sat, &QComboBox::currentIndexChanged, [this, changed] (int index) { satelliteIndex = index; picture.reset(); reloadNeeded = true; loading = false; ++generation; changed(); });
    QObject::connect(kind, &QComboBox::currentIndexChanged, [this, changed] (int index) { product = index; picture.reset(); reloadNeeded = true; loading = false; ++generation; changed(); });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { brightness = value; changed(); });
    return widget;
}
