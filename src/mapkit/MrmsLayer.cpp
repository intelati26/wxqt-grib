// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MrmsLayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include "util/Utility.h"

const UtilityMrms::Product& MrmsLayer::product() const {
    const auto& all = UtilityMrms::products();
    return all[static_cast<size_t>(std::clamp(productIndex, 0, static_cast<int>(all.size()) - 1))];
}

string MrmsLayer::summary() const {
    if (loading && !have) return "reading the MRMS scan...";
    if (!error.empty()) return error;
    if (!have) return {};
    return "MRMS " + product().label + " " + frame.utc.toString("HH:mm").toStdString() + "Z";
}

void MrmsLayer::rebuildColors() {
    colors = UtilityMrms::colorTable(product(), frame);
    for (auto& c : colors) {
        c = qRgba(qRed(c), qGreen(c), qBlue(c), qAlpha(c) * opacity / 100);
    }
    // premultiplied, as the image format is
    for (auto& c : colors) {
        const int a = qAlpha(c);
        c = qRgba(qRed(c) * a / 255, qGreen(c) * a / 255, qBlue(c) * a / 255, a);
    }
}

void MrmsLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const int mine = ++generation;
    struct Loaded {
        std::vector<UtilityMrms::Scan> scans;
        UtilityMrms::Frame frame;
        string error;
    };
    auto result = std::make_shared<Loaded>();
    const auto chosen = product();
    host.background(
        [chosen, result] {
            if (UtilityMrms::scans(chosen, result->scans, result->error) && !result->scans.empty()) {
                UtilityMrms::frame(chosen, result->scans.back(), result->frame, result->error);
            }
        },
        [this, &host, result, mine] {
            loading = false;
            if (mine != generation) {
                return;
            }
            if (!result->error.empty()) {
                error = result->error;
            } else {
                error.clear();
                frame = result->frame;
                indices = frame.indices();
                rebuildColors();
                have = true;
            }
            host.redraw();
        });
}

// every device pixel looks its cell up in the scan's grid: the picture is as sharp at any zoom as the data
void MrmsLayer::paint(QPainter& painter, MapHost& host) {
    if (!have || indices.isEmpty()) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const auto& grid = frame.grid;
    const double ratio = painter.device()->devicePixelRatioF();
    const int pixelWidth = std::max(1, static_cast<int>(std::lround(view.map()->width() * ratio)));
    const int pixelHeight = std::max(1, static_cast<int>(std::lround(view.map()->height() * ratio)));
    vector<int> columnOf(static_cast<size_t>(pixelWidth));
    for (int i = 0; i < pixelWidth; i++) {
        const double u = (i + 0.5) / ratio * 1000.0 / view.map()->width() - 500.0;
        const double lon = ((u - t.xPos) / t.zoom - t.bx) / t.ax;
        const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
        columnOf[static_cast<size_t>(i)] = column >= 0 && column < grid.columns ? column : -1;
    }
    QImage image{pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied};
    const auto * cells = reinterpret_cast<const uchar *>(indices.constData());
    for (int j = 0; j < pixelHeight; j++) {
        const double v = (j + 0.5) / ratio * 1000.0 / view.map()->height() - 250.0;
        const double mercator = ((v - t.yPos) / t.zoom - t.by) / t.ay;
        const double lat = std::atan(std::sinh(mercator * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
        if (row < 0 || row >= grid.rows) {
            std::fill(out, out + pixelWidth, qRgba(0, 0, 0, 0));
            continue;
        }
        const auto * source = cells + static_cast<qsizetype>(row) * grid.columns;
        for (int i = 0; i < pixelWidth; i++) {
            const int column = columnOf[static_cast<size_t>(i)];
            out[i] = column < 0 ? qRgba(0, 0, 0, 0) : colors[source[column]];
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRectF{-500.0, -250.0, 1000.0, 1000.0 * view.map()->height() / std::max(1, view.map()->width())}, image, QRectF{image.rect()});
}

MapHit MrmsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;   // under every mark
    if (!have || indices.isEmpty()) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const auto& grid = frame.grid;
    const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
    const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
    if (column < 0 || column >= grid.columns || row < 0 || row >= grid.rows) {
        return hit;
    }
    const int index = static_cast<uchar>(indices[static_cast<qsizetype>(row) * grid.columns + column]);
    if (index == 0) {
        return hit;
    }
    const double value = UtilityMrms::shown(product(), frame.valueAt(index), us);
    hit.distance = 0.0;
    hit.text = QString::fromStdString(product().label) + ": " + QString::number(value, 'g', 3) + " " + QString::fromStdString(UtilityMrms::unitsShown(product(), us)) + "\n" +
        QString::number(lat, 'f', 2) + " N, " + QString::number(-lon, 'f', 2) + " W   " + frame.utc.toString("HH:mm") + "Z";
    return hit;
}

vector<MapLegendRow> MrmsLayer::legend() const {
    MapLegendRow row;
    row.title = QString::fromStdString(product().label) + " (" + QString::fromStdString(UtilityMrms::unitsShown(product(), us)) + "):";
    const auto& stops = product().stops;
    const double lo = frame.validMin;
    const double span = frame.step * 254.0;
    for (size_t i = 0; i < stops.size(); i++) {
        const double value = product().autoRange ? lo + stops[i].value * span : stops[i].value;
        row.entries.push_back({MapLegendEntry::Square, QColor{stops[i].r, stops[i].g, stops[i].b}, QString::number(UtilityMrms::shown(product(), value, us), 'g', 3)});
    }
    return {row};
}

QWidget * MrmsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Product:", widget});
    auto * combo = new QComboBox{widget};
    for (const auto& p : UtilityMrms::products()) {
        combo->addItem(QString::fromStdString((p.group.empty() ? "" : p.group + ": ") + p.label));
    }
    combo->setCurrentIndex(productIndex);
    layout->addWidget(combo);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    auto * units = new QCheckBox{"US units (inches, kft)", widget};
    units->setChecked(us);
    layout->addWidget(units);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        productIndex = index;
        have = false;
        indices.clear();
        error.clear();
        loading = false;   // a scan of the old product still on its way is dropped when it arrives (the generation moved on)
        ++generation;
        reloadNeeded = true;
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; if (have) rebuildColors(); changed(); });
    QObject::connect(units, &QCheckBox::toggled, [this, changed] (bool on) { us = on; changed(); });
    return widget;
}
