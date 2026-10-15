// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GoesHistoryLayer.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPainter>
#include <QProcess>
#include <QSlider>
#include <QStandardPaths>
#include <QTimeZone>
#include <QVBoxLayout>
#include "models/UtilityGrib.h"
#include "radar/MapView.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

namespace {
    const char * kinds[] = {"ir", "vis", "wv"};
    const char * labels[] = {"Infrared", "Visible", "Water vapor"};
    // the composite's own frame: 4 km (0.04 degree) cells over 130 W - 60 W, 22 N - 52 N
    constexpr double west = -130.0, east = -60.0, north = 52.0, south = 22.0;
    constexpr long step = 15 * 60;
}

GoesHistoryLayer::GoesHistoryLayer() {
    product = std::clamp(Utility::readPrefInt("MM_GOESHIST_PRODUCT", 0), 0, 2);
    enhance = Utility::readPref("MM_GOESHIST_ENHANCE", "true") == "true";
}

QDateTime GoesHistoryLayer::centre() const {
    if (asOf.isValid()) {
        return asOf.toUTC();
    }
    return QDateTime::currentDateTimeUtc().addSecs(-40 * 60);
}

vector<long> GoesHistoryLayer::times() const {
    vector<long> out;
    const long middle = centre().toSecsSinceEpoch() / step * step;
    const int before = asOf.isValid() ? 12 : 24, after = asOf.isValid() ? 12 : 0;   // three hours either side of a time of the past; the last six hours otherwise
    for (int i = -before; i <= after; i++) {
        out.push_back(middle + i * step);
    }
    return out;
}

string GoesHistoryLayer::urlOf(long seconds) const {
    const auto t = QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc());
    return "https://mesonet.agron.iastate.edu/archive/data/" + t.toString("yyyy/MM/dd").toStdString() + "/GIS/sat/conus_goes_" + kinds[product] + "4km_" + t.toString("HHmm").toStdString() + ".tif";
}

string GoesHistoryLayer::timeText() const {
    const long s = shown != 0 ? shown : times().back();
    return string{labels[product]} + "  " + QDateTime::fromSecsSinceEpoch(s, QTimeZone::utc()).toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z  GOES (4 km)";
}

string GoesHistoryLayer::summary() const {
    const long s = shown != 0 ? shown : (times().empty() ? 0 : times().back());
    if (!error.empty()) {
        return error;
    }
    return string{labels[product]} + "  " + QDateTime::fromSecsSinceEpoch(s, QTimeZone::utc()).toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
}

// the GeoTIFF as an image: the Qt TIFF reader when the build has it, GDAL's translate otherwise; the enhancement of the cold cloud tops of the infrared is applied here
QImage GoesHistoryLayer::decode(const QByteArray& bytes) const {
    QImage gray = QImage::fromData(bytes, "TIFF");
    if (gray.isNull()) {
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/wxqt_goes";
        QDir{}.mkpath(folder);
        const QString in = folder + "/h.tif", out = folder + "/h.png";
        QFile file{in};
        if (file.open(QIODevice::WriteOnly)) {
            file.write(bytes);
            file.close();
            QProcess process;
            process.start(QString::fromStdString(UtilityGrib::gdalBinDir()) + "/gdal_translate", {"-q", "-of", "PNG", in, out});
            process.waitForFinished(30000);
            gray = QImage{out};
        }
    }
    if (gray.isNull()) {
        return {};
    }
    gray = gray.convertToFormat(QImage::Format_Grayscale8);
    QImage result{gray.width(), gray.height(), QImage::Format_ARGB32_Premultiplied};
    const bool colored = enhance && product == 0;
    for (int y = 0; y < gray.height(); y++) {
        const auto * in = gray.constScanLine(y);
        auto * out = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < gray.width(); x++) {
            const int v = in[x];
            if (v == 0) {
                out[x] = qRgba(0, 0, 0, 0);   // outside the composite
            } else if (colored && v >= 150) {   // the cold cloud tops: blue, cyan, green, yellow, orange, red, magenta
                static const QColor ramp[] = {QColor{70, 70, 230}, QColor{40, 200, 230}, QColor{60, 220, 70}, QColor{250, 240, 40}, QColor{250, 150, 30}, QColor{230, 40, 40}, QColor{230, 40, 220}};
                const double f = std::min(0.999, (v - 150) / 70.0) * 6.0;
                const int k = static_cast<int>(f);
                const double m = f - k;
                const QColor a = ramp[k], b = ramp[k + 1];
                out[x] = qRgb(static_cast<int>(a.red() + (b.red() - a.red()) * m), static_cast<int>(a.green() + (b.green() - a.green()) * m), static_cast<int>(a.blue() + (b.blue() - a.blue()) * m));
            } else {
                out[x] = qRgb(v, v, v);
            }
        }
    }
    return result;
}

void GoesHistoryLayer::request(MapHost& host, long seconds) {
    const string key = string{kinds[product]} + "|" + std::to_string(seconds);
    if (frames.count(key) > 0 || asked.count(key) > 0) {
        return;
    }
    asked.insert(key);
    const string url = urlOf(seconds);
    auto bytes = std::make_shared<QByteArray>();
    host.background([url, bytes] { *bytes = UtilityIO::downloadAsByteArray(url); },
                    [this, key, bytes, &host] {
                        if (!bytes->isEmpty()) {
                            const auto image = decode(*bytes);
                            if (!image.isNull()) {
                                frames[key] = image;
                                error.clear();
                            }
                        } else {
                            error = "no GOES image for that time in the archive";
                        }
                        host.redraw();
                    });
}

void GoesHistoryLayer::prepareTimes(MapHost& host) {
    for (const long s : times()) {
        request(host, s);
    }
    host.timesChanged();
}

void GoesHistoryLayer::showTime(long seconds, MapHost& host) {
    shown = seconds == 0 ? 0 : seconds / step * step;
    host.redraw();
}

void GoesHistoryLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    shown = asOf.toSecsSinceEpoch() / step * step;
    frames.clear();
    asked.clear();
    host.timesChanged();
    host.redraw();
}

void GoesHistoryLayer::paint(QPainter& painter, MapHost& host) {
    const auto all = times();
    if (all.empty()) {
        return;
    }
    long seconds = shown != 0 ? shown : all.back();
    if (asOf.isValid() && shown == 0) {
        seconds = asOf.toSecsSinceEpoch() / step * step;
    }
    request(host, seconds);
    const auto found = frames.find(string{kinds[product]} + "|" + std::to_string(seconds));
    if (found == frames.end()) {
        return;
    }
    const auto& image = found->second;
    const auto t = host.view().transform();
    // plate carree cells on a Mercator map: drawn in strips of rows, each strip straight between its own latitudes
    painter.save();
    painter.setOpacity(opacity / 100.0);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const int strips = 60;
    const int rows = image.height();
    for (int s = 0; s < strips; s++) {
        const int r0 = rows * s / strips, r1 = rows * (s + 1) / strips;
        const double lat0 = north - (north - south) * r0 / rows, lat1 = north - (north - south) * r1 / rows;
        const QPointF a = t(lat0, west), b = t(lat1, east);
        painter.drawImage(QRectF{a, b}.normalized().adjusted(0.0, -0.4, 0.0, 0.4), image, QRectF{0.0, static_cast<double>(r0), static_cast<double>(image.width()), static_cast<double>(r1 - r0)});
    }
    painter.restore();
    if (frames.size() > 150) {
        frames.clear();
        asked.clear();
    }
}

QWidget * GoesHistoryLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Channel:", widget});
    auto * combo = new QComboBox{widget};
    combo->addItems({"Infrared (cloud-top temperature)", "Visible (daylight)", "Water vapor"});
    combo->setCurrentIndex(product);
    layout->addWidget(combo);
    auto * color = new QCheckBox{"Color the cold cloud tops (infrared)", widget};
    color->setChecked(enhance);
    layout->addWidget(color);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        product = index;
        Utility::writePrefInt("MM_GOESHIST_PRODUCT", index);
        changed();
    });
    QObject::connect(color, &QCheckBox::toggled, [this, changed] (bool on) {
        enhance = on;
        Utility::writePref("MM_GOESHIST_ENHANCE", on ? "true" : "false");
        frames.clear();
        asked.clear();
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
