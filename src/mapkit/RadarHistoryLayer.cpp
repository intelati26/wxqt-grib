// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/RadarHistoryLayer.h"
#include <algorithm>
#include <cmath>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QTimeZone>
#include <QVBoxLayout>
#include "radar/MapView.h"
#include "util/UtilityIO.h"

namespace {
    constexpr long step = 5 * 60;
}

RadarHistoryLayer::RadarHistoryLayer() = default;

QDateTime RadarHistoryLayer::centre() const {
    return asOf.isValid() ? asOf.toUTC() : QDateTime::currentDateTimeUtc().addSecs(-15 * 60);
}

vector<long> RadarHistoryLayer::times() const {
    vector<long> out;
    if (rangeFrom != 0) {   // a loop from one time to another: at most 96 frames, every few steps when it is long
        const long count = (rangeTo - rangeFrom) / step + 1;
        const long every = std::max(1L, (count + 95) / 96);
        for (long t = rangeFrom; t <= rangeTo; t += every * step) {
            out.push_back(t);
        }
        return out;
    }
    const long middle = centre().toSecsSinceEpoch() / step * step;
    const int before = asOf.isValid() ? 12 : 12, after = asOf.isValid() ? 12 : 0;
    for (int i = -before; i <= after; i++) {
        out.push_back(middle + i * step);
    }
    return out;
}

string RadarHistoryLayer::urlOf(const char * kind, long seconds, const char * extension) {
    const auto t = QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc());
    return "https://mesonet.agron.iastate.edu/archive/data/" + t.toString("yyyy/MM/dd").toStdString() + "/GIS/uscomp/" + kind + "_" + t.toString("yyyyMMddHHmm").toStdString() + "." + extension;
}

string RadarHistoryLayer::timeText() const {
    const long s = shown != 0 ? shown : times().back();
    return QDateTime::fromSecsSinceEpoch(s, QTimeZone::utc()).toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z  radar mosaic";
}

string RadarHistoryLayer::summary() const {
    if (frames.empty() && error.empty()) {
        return asked.empty() ? string{} : "reading the radar mosaic...";
    }
    return error;
}

// the world file: x cell, 0, 0, -y cell, then the centre of the upper left pixel
bool RadarHistoryLayer::decode(const QByteArray& png, const QByteArray& world, Frame& f) {
    const auto lines = QString::fromLatin1(world).split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 6) {
        return false;
    }
    const double cellX = lines[0].trimmed().toDouble(), cellY = -lines[3].trimmed().toDouble();
    if (cellX <= 0.0 || cellY <= 0.0) {
        return false;
    }
    QImage source = QImage::fromData(png, "PNG");
    if (source.isNull() || source.format() != QImage::Format_Indexed8) {
        return false;
    }
    // the 12,000 pixel wide picture is thinned to about 1.6 km: the strongest echo of each block, so that small cells are not lost
    const int factor = std::max(1, static_cast<int>(std::lround(0.015 / cellX)));
    if (factor == 1) {
        f.image = source;
    } else {
        const int w = source.width() / factor, h = source.height() / factor;
        QImage small{w, h, QImage::Format_Indexed8};
        small.setColorTable(source.colorTable());
        for (int y = 0; y < h; y++) {
            auto * out = small.scanLine(y);
            for (int x = 0; x < w; x++) {
                uchar best = 0;
                for (int j = 0; j < factor; j++) {
                    const auto * in = source.constScanLine(y * factor + j) + x * factor;
                    for (int i = 0; i < factor; i++) {
                        best = std::max(best, in[i]);
                    }
                }
                out[x] = best;
            }
        }
        f.image = small;
    }
    // index 0 is "no echo": clear
    auto table = f.image.colorTable();
    if (!table.isEmpty()) {
        table[0] = qRgba(0, 0, 0, 0);
        f.image.setColorTable(table);
    }
    f.cellX = cellX * factor;
    f.cellY = cellY * factor;
    f.west = lines[4].trimmed().toDouble() - cellX / 2.0;
    f.north = lines[5].trimmed().toDouble() + cellY / 2.0;
    return true;
}

void RadarHistoryLayer::request(MapHost& host, long seconds) {
    if (frames.count(seconds) > 0 || asked.count(seconds) > 0) {
        return;
    }
    asked.insert(seconds);
    struct Got {
        Frame frame;
        bool ok{false};
    };
    auto got = std::make_shared<Got>();
    host.background(
        [got, seconds] {
            for (const char * kind : {"n0q", "n0r"}) {   // the 8-bit picture from 2010; the 4-bit before
                const auto png = UtilityIO::downloadAsByteArray(urlOf(kind, seconds, "png"));
                if (png.isEmpty()) {
                    continue;
                }
                const auto world = UtilityIO::downloadAsByteArray(urlOf(kind, seconds, "wld"));
                if (decode(png, world, got->frame)) {
                    got->frame.kind = kind;
                    got->ok = true;
                    return;
                }
            }
        },
        [this, got, seconds, &host] {
            if (got->ok) {
                frames[seconds] = std::move(got->frame);
                error.clear();
            } else {
                missing++;
                if (frames.empty() && missing >= 3) {
                    error = "no radar mosaic in the archive for that time";
                }
            }
            while (frames.size() > 30) {
                frames.erase(frames.begin() == frames.find(shown) ? std::next(frames.begin()) : frames.begin());
            }
            host.timesChanged();
            host.redraw();
        });
}

void RadarHistoryLayer::prepareTimes(MapHost& host) {
    for (const long s : times()) {
        request(host, s);
    }
    host.timesChanged();
}

void RadarHistoryLayer::showTime(long seconds, MapHost& host) {
    shown = seconds == 0 ? 0 : seconds / step * step;
    host.redraw();
}

void RadarHistoryLayer::setRange(const QDateTime& from, const QDateTime& to, MapHost& host) {
    setAsOf(to, host);
    rangeFrom = from.toUTC().toSecsSinceEpoch() / step * step;
    rangeTo = std::max(rangeFrom, static_cast<long>(to.toUTC().toSecsSinceEpoch() / step * step));
    shown = rangeFrom;
    host.timesChanged();
    host.redraw();
}

void RadarHistoryLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    rangeFrom = 0;
    asOf = utc.toUTC();
    shown = asOf.toSecsSinceEpoch() / step * step;
    frames.clear();
    asked.clear();
    missing = 0;
    error.clear();
    host.timesChanged();
    host.redraw();
}

void RadarHistoryLayer::paint(QPainter& painter, MapHost& host) {
    const auto all = times();
    long seconds = shown != 0 ? shown : all.back();
    request(host, seconds);
    auto found = frames.find(seconds);
    if (found == frames.end()) {   // a frame the archive lacks: the nearest one before it that is there
        found = frames.upper_bound(seconds);
        if (found == frames.begin()) {
            return;
        }
        --found;
        if (seconds - found->first > 20 * 60) {
            return;
        }
    }
    const auto& f = found->second;
    const auto t = host.view().transform();
    painter.save();
    painter.setOpacity(opacity / 100.0);
    const int strips = 60, rows = f.image.height();
    const double east = f.west + f.cellX * f.image.width();
    for (int s = 0; s < strips; s++) {
        const int r0 = rows * s / strips, r1 = rows * (s + 1) / strips;
        const double lat0 = f.north - f.cellY * r0, lat1 = f.north - f.cellY * r1;
        const QPointF a = t(lat0, f.west), b = t(lat1, east);
        painter.drawImage(QRectF{a, b}.normalized().adjusted(0.0, -0.4, 0.0, 0.4), f.image, QRectF{0.0, static_cast<double>(r0), static_cast<double>(f.image.width()), static_cast<double>(r1 - r0)});
    }
    painter.restore();
}

QWidget * RadarHistoryLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
