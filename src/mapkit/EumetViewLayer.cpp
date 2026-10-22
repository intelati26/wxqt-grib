// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/EumetViewLayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QUrl>
#include <QPainter>
#include <QSlider>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include "objects/URL.h"
#include "radar/MapView.h"
#include "util/Utility.h"

namespace {
    const char * const wms = "https://view.eumetsat.int/geoserver/wms";

    double mercatorY(double lat) {
        const double r = std::clamp(lat, -85.0, 85.0) * std::numbers::pi / 180.0;
        return std::log(std::tan(r) + 1.0 / std::cos(r));
    }

    QString isoOf(const QDateTime& t) {
        return t.toUTC().toString("yyyy-MM-dd'T'HH:mm:ss'Z'");
    }
}

const std::vector<EumetViewLayer::Product>& EumetViewLayer::products() {
    static const std::vector<Product> all{
        {"mtg_fd:rgb_geocolour", "Meteosat (MTG-I) geocolor, 0 degrees", false, 10},
        {"mtg_fd:ir105_hrfi", "Meteosat (MTG-I) infrared 10.5 micrometers, 0 degrees", false, 10},
        {"mtg_fd:vis06_hrfi", "Meteosat (MTG-I) visible 0.6 micrometers, 0 degrees", false, 10},
        {"eps:m01_ascat_wind", "Metop-B ASCAT coastal winds (12.5 km)", true, 0},
        {"eps:m03_ascat_wind", "Metop-C ASCAT coastal winds (12.5 km)", true, 0},
        {"copernicus:sentinel3a_olci_l1_rgb_fullres", "Sentinel-3A OLCI true color", true, 0},
        {"copernicus:sentinel3b_olci_l1_rgb_fullres", "Sentinel-3B OLCI true color", true, 0},
        {"copernicus:daily_sentinel3ab_olci_l1_rgb_fulres", "Sentinel-3A and 3B OLCI true color, a day's passes", true, 0},
    };
    return all;
}

EumetViewLayer::EumetViewLayer() {
    productIndex = std::clamp(Utility::readPrefInt("MM_EUMET_PRODUCT", 0), 0, static_cast<int>(products().size()) - 1);
    windowHours = std::clamp(Utility::readPrefInt("MM_EUMET_HOURS", 12), 1, 48);
}

// the newest times of the layers, from the service's capabilities (a long document: read once an hour)
void EumetViewLayer::refresh(MapHost& host) {
    const auto now = QDateTime::currentDateTimeUtc();
    if (capabilitiesAsked && capabilitiesAt.isValid() && capabilitiesAt.secsTo(now) < 3600) {
        host.redraw();
        return;
    }
    capabilitiesAsked = true;
    auto text = std::make_shared<QByteArray>();
    fetching++;
    host.background([text] { *text = URL::getBytes(string{wms} + "?service=WMS&version=1.3.0&request=GetCapabilities"); }, [this, text, now, &host] {
        fetching--;
        QXmlStreamReader xml{*text};
        QString name;
        bool inName = false, inDimension = false, timeDimension = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                inName = xml.name() == QLatin1String("Name");
                inDimension = xml.name() == QLatin1String("Dimension");
                timeDimension = inDimension && xml.attributes().value("name") == QLatin1String("time");
                if (timeDimension) {
                    const auto extent = xml.readElementText().trimmed();   // "start/end/period" (or a list)
                    const auto parts = extent.split('/');
                    const auto last = parts.size() >= 2 ? parts[1] : extent.split(',').last();
                    const auto when = QDateTime::fromString(last, Qt::ISODate);
                    if (when.isValid() && !name.isEmpty()) {
                        latest[name.toStdString()] = when.toUTC();
                    }
                    timeDimension = false;
                }
            } else if (xml.isCharacters() && inName) {
                const auto candidate = xml.text().toString().trimmed();
                if (candidate.contains(':') || candidate.startsWith("eps")) {
                    name = candidate;
                }
                inName = false;
            }
        }
        capabilitiesAt = now;
        host.redraw();
    });
}

QDateTime EumetViewLayer::endTime() const {
    if (asOf.isValid()) {
        return asOf;
    }
    const auto found = latest.find(product().layer);
    return found != latest.end() ? found->second : QDateTime::currentDateTimeUtc();
}

string EumetViewLayer::timeArgument() const {
    auto end = endTime().toUTC();
    if (!product().swath) {   // a picture every ten minutes: the slot at or before the time
        const int step = product().stepMinutes;
        const auto t = end.time();
        end.setTime(QTime{t.hour(), t.minute() / step * step, 0});
        return isoOf(end).toStdString();
    }
    return (isoOf(end.addSecs(-3600LL * windowHours)) + "/" + isoOf(end)).toStdString();
}

string EumetViewLayer::imageUrl(double west, double south, double east, double north, int width, int height, const string& time) const {
    const auto x = [] (double lon) { return lon / 180.0 * 20037508.342789244; };
    const auto y = [] (double lat) { return mercatorY(lat) / std::numbers::pi * 20037508.342789244; };
    char box[160];
    std::snprintf(box, sizeof box, "%.1f,%.1f,%.1f,%.1f", x(west), y(south), x(east), y(north));
    return string{wms} + "?service=WMS&version=1.3.0&request=GetMap&styles=&format=image/png&transparent=true&crs=EPSG:3857&layers=" + product().layer + "&bbox=" + box + "&width=" + std::to_string(width) +
           "&height=" + std::to_string(height) + "&time=" + time;
}

void EumetViewLayer::paint(QPainter& painter, MapHost& host) {
    auto& view = host.view();
    const auto topLeft = view.toLatLon(QPointF{0.0, 0.0});
    const auto bottomRight = view.toLatLon(QPointF{static_cast<double>(view.pixelWidth()), static_cast<double>(view.pixelHeight())});
    double north = std::min(topLeft.first, 85.0), south = std::max(bottomRight.first, -85.0);
    double west = topLeft.second, east = bottomRight.second;
    if (east - west < 1.0e-6 || east - west > 360.0) {
        west = -180.0;
        east = 180.0;
    }
    // the box on a one degree grid (a small pan asks for nothing more), and the picture as many pixels as the screen has for it
    west = std::floor(west);
    east = std::ceil(east);
    south = std::floor(south);
    north = std::ceil(north);
    const double pixelsPerDegree = view.pixelWidth() / std::max(1.0e-6, bottomRight.second - topLeft.second > 0 ? bottomRight.second - topLeft.second : 360.0);
    const double spanX = (east - west) * std::numbers::pi / 180.0, spanY = mercatorY(north) - mercatorY(south);
    int width = std::clamp(static_cast<int>(std::lround((east - west) * pixelsPerDegree)), 64, 1600);
    int height = std::clamp(static_cast<int>(std::lround(width * spanY / std::max(1.0e-9, spanX))), 64, 1600);
    shownBox = QString::fromStdString(string{product().layer});
    if (product().swath) {
        askActual(host, west, south, east, north);
    }
    std::vector<string> times;   // the pictures laid together, the oldest first
    if (perPass()) {
        times = granuleTimes;
    } else {
        times.push_back(timeArgument());
    }
    const auto t = view.transform();
    const QPointF a = t(north, west), b = t(south, east);
    for (const auto& time : times) {
        const string key = string{product().layer} + "|" + time + "|" + std::to_string(static_cast<int>(west)) + "," + std::to_string(static_cast<int>(south)) + "," + std::to_string(static_cast<int>(east)) + "," +
                           std::to_string(static_cast<int>(north)) + "|" + std::to_string(width) + "x" + std::to_string(height);
        const auto found = images.find(key);
        if (found == images.end()) {
            if (asked.insert(key).second) {
                const auto url = imageUrl(west, south, east, north, width, height, time);
                auto bytes = std::make_shared<QByteArray>();
                fetching++;
                host.background([bytes, url] { *bytes = URL::getBytes(url); }, [this, bytes, key, &host] {
                    fetching--;
                    QImage image;
                    if (image.loadFromData(*bytes)) {
                        images[key] = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                    } else {
                        images[key] = QImage{};   // an error page, or nothing: not asked again for this view
                    }
                    host.redraw();
                });
            }
            continue;
        }
        if (found->second.isNull()) {
            continue;
        }
        painter.save();
        painter.setOpacity(opacity / 100.0);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(QRectF{a, b}.normalized(), found->second);
        painter.restore();
    }
    if (images.size() > 60) {   // a session's pictures are a few MB each: the older ones go
        images.clear();
        asked.clear();
    }
}

void EumetViewLayer::askActual(MapHost& host, double west, double south, double east, double north) {
    const auto end = endTime().toUTC();
    const auto start = end.addSecs(-3600LL * windowHours);
    const string layer = product().layer;
    const string key = layer + "|" + isoOf(start).toStdString() + "|" + isoOf(end).toStdString() + "|" + std::to_string(static_cast<int>(west)) + "," + std::to_string(static_cast<int>(south)) + "," + std::to_string(static_cast<int>(east)) + "," +
                       std::to_string(static_cast<int>(north));
    if (key == actualKey || actualBusy) {
        return;
    }
    actualBusy = true;
    granuleTimes.clear();
    actual.clear();   // (the one of another picture, window or view is not this one's)
    const auto a = isoOf(start).toStdString(), b = isoOf(end).toStdString();
    const double lat = (south + north) / 2.0, lon = (west + east) / 2.0;
    auto text = std::make_shared<string>();
    auto times = std::make_shared<std::vector<string>>();
    host.background([text, times, layer, a, b, west, south, east, north, lat, lon] {
        const auto encode = [] (const string& s) { return QUrl::toPercentEncoding(QString::fromStdString(s)).toStdString(); };
        const auto clock = [] (const QString& iso) { return QDateTime::fromString(iso, Qt::ISODate).toUTC().toString("HH:mm").toStdString() + "Z (" + QDateTime::fromString(iso, Qt::ISODate).toUTC().toString("MM-dd").toStdString() + ")"; };
        if (layer.compare(0, 4, "eps:") == 0) {   // the measurements' own times: the earliest and the latest in the view
            const string filter = "BBOX(geom," + std::to_string(west) + "," + std::to_string(south) + "," + std::to_string(east) + "," + std::to_string(north) + ") AND time DURING " + a + "/" + b;
            string first, last;
            for (const char * order : {"time A", "time D"}) {
                const auto reply = URL::getText("https://view.eumetsat.int/geoserver/wfs?service=WFS&version=2.0.0&request=GetFeature&outputFormat=application/json&typeNames=" + layer + "&propertyName=time&count=1&sortBy=" +
                                                encode(order) + "&cql_filter=" + encode(filter));
                const auto features = QJsonDocument::fromJson(QByteArray::fromStdString(reply)).object().value("features").toArray();
                if (!features.isEmpty()) {
                    (string{order} == "time A" ? first : last) = clock(features.first().toObject().value("properties").toObject().value("time").toString());
                }
            }
            *text = first.empty() ? "no winds in this view in the window" : (first == last ? "measured " + first : "measured from " + first + " to " + last) + ", from the data";
            return;
        }
        // Sentinel-3 OLCI: the products over the middle of the view in the window
        const string satellite = layer.find("sentinel3a") != string::npos ? "S3A_OL_1_EFR" : layer.find("sentinel3b") != string::npos ? "S3B_OL_1_EFR" : "OL_1_EFR";
        const string filter = "Collection/Name eq 'SENTINEL-3' and contains(Name,'" + satellite + "') and OData.CSC.Intersects(area=geography'SRID=4326;POINT(" + std::to_string(lon) + " " + std::to_string(lat) + ")') and ContentDate/Start gt " + a +
                              " and ContentDate/Start lt " + b;
        const auto reply = URL::getText("https://catalogue.dataspace.copernicus.eu/odata/v1/Products?$filter=" + encode(filter) + "&$orderby=" + encode("ContentDate/Start asc") + "&$top=8");
        const auto products = QJsonDocument::fromJson(QByteArray::fromStdString(reply)).object().value("value").toArray();
        string list;
        for (const auto& value : products) {
            {   // the middle of the product: the picture's own time
                const auto content = value.toObject().value("ContentDate").toObject();
                const auto s = QDateTime::fromString(content.value("Start").toString(), Qt::ISODate), e = QDateTime::fromString(content.value("End").toString(), Qt::ISODate);
                times->push_back(isoOf(s.addSecs(s.secsTo(e) / 2)).toStdString());
            }
            const auto product = value.toObject();
            const auto content = product.value("ContentDate").toObject();
            const auto name = product.value("Name").toString();
            const auto s = QDateTime::fromString(content.value("Start").toString(), Qt::ISODate).toUTC(), e = QDateTime::fromString(content.value("End").toString(), Qt::ISODate).toUTC();
            list += (list.empty() ? "" : ", ") + name.left(3).toStdString() + " " + s.toString("HH:mm").toStdString() + "-" + e.toString("HH:mm").toStdString() + "Z (" + s.toString("MM-dd").toStdString() + ")";
        }
        *text = list.empty() ? "no pass over the middle of the view in the window" : "scanned " + list + ", over the middle of the view (Copernicus Data Space)";
    }, [this, text, times, key, &host] {
        actualBusy = false;
        actualKey = key;
        actual = *text;
        granuleTimes = *times;
        host.timesChanged();
        host.redraw();
    });
}

string EumetViewLayer::summary() const {
    return string{product().label} + (fetching > 0 ? " (reading)" : "");
}

string EumetViewLayer::timeText() const {
    const auto end = endTime();
    const auto found = latest.find(product().layer);
    const string newest = found != latest.end() ? found->second.toString("yyyy-MM-dd HH:mm").toStdString() + "Z" : string{};
    if (product().swath && !actual.empty()) {
        return string{product().label} + "  " + actual;
    }
    if (product().swath) {
        const auto start = end.addSecs(-3600LL * windowHours);
        return string{product().label} + "  passes from " + start.toString("MM-dd HH:mm").toStdString() + " to " + end.toString("MM-dd HH:mm").toStdString() + "Z" + (newest.empty() ? string{} : "  (newest in the service " + newest + ")");
    }
    auto slot = end.toUTC();
    const int step = product().stepMinutes;
    slot.setTime(QTime{slot.time().hour(), slot.time().minute() / step * step, 0});
    return string{product().label} + "  " + slot.toString("yyyy-MM-dd HH:mm").toStdString() + "Z" + (newest.empty() || asOf.isValid() ? string{} : "  (newest in the service " + newest + ")");
}

void EumetViewLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    host.redraw();
}

QWidget * EumetViewLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Picture:", widget});
    auto * combo = new QComboBox{widget};
    for (const auto& p : products()) {
        combo->addItem(p.label);
    }
    combo->setCurrentIndex(productIndex);
    layout->addWidget(combo);
    layout->addWidget(new QLabel{"Passes laid together (hours before the time shown):", widget});
    auto * window = new QComboBox{widget};
    for (const int h : {3, 6, 12, 24, 48}) {
        window->addItem(QString::number(h) + " hours", h);
        if (h == windowHours) {
            window->setCurrentIndex(window->count() - 1);
        }
    }
    layout->addWidget(window);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        productIndex = index;
        Utility::writePrefInt("MM_EUMET_PRODUCT", index);
        changed();
    });
    QObject::connect(window, &QComboBox::currentIndexChanged, [this, window, changed] (int) {
        windowHours = window->currentData().toInt();
        Utility::writePrefInt("MM_EUMET_HOURS", windowHours);
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
