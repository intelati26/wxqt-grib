// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MrmsOverlayLayers.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include "radar/MapView.h"
#include "ui/UiStandards.h"

namespace {
    const QColor strikeColors[4] = {QColor{255, 255, 255}, QColor{255, 235, 40}, QColor{255, 150, 20}, QColor{235, 40, 40}};
    const QColor chanceColors[4] = {QColor{120, 200, 255}, QColor{255, 235, 40}, QColor{255, 150, 20}, QColor{235, 40, 40}};
    const char * strikeNames[4] = {"last minute", "1-5 min", "5-15 min", "15-30 min"};
    const char * chanceNames[4] = {"10%", "30%", "50%", "70%"};
}

using Kind = MrmsOverlayLayer::Kind;

MrmsOverlayLayer::MrmsOverlayLayer(Kind kind) : kind{kind} {}

string MrmsOverlayLayer::id() const {
    switch (kind) {
        case Kind::Lightning: return "radar/mrms-lightning";
        case Kind::Chance: return "radar/mrms-chance";
        case Kind::Hail: return "radar/mrms-hail";
        default: return "radar/mrms-rotation";
    }
}

string MrmsOverlayLayer::path() const {
    switch (kind) {
        case Kind::Lightning: return "Radar and precipitation/MRMS lightning strikes (by age)";
        case Kind::Chance: return "Radar and precipitation/MRMS lightning chance (contours)";
        case Kind::Hail: return "Radar and precipitation/MRMS hail swaths (1 in and up)";
        default: return "Radar and precipitation/MRMS rotation tracks";
    }
}

string MrmsOverlayLayer::tip() const {
    switch (kind) {
        case Kind::Lightning: return "Cloud-to-ground strikes of the last minute to 30 minutes, each cell with a strike colored by the shortest period it is in";
        case Kind::Chance: return "The chance of lightning in the next 30 or 60 minutes, as lines at 10, 30, 50 and 70 percent";
        case Kind::Hail: return "Where the largest estimated hail (MESH) of the period was at least marginally severe: 1 inch and up, colored by size";
        default: return "Where the strongest low-level (0-2 km) or mid-level (3-6 km) rotation passed in the period: supercell tracks, from 8 thousandths per second up";
    }
}

string MrmsOverlayLayer::summary() const {
    if (loading && !have) return "reading MRMS...";
    if (!have) return {};
    return "MRMS " + utc.toString("HH:mm").toStdString() + "Z";
}

void MrmsOverlayLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const int mine = ++generation;
    const int which = choice;
    const Kind what = kind;
    struct Loaded {
        MrmsOverlays::Lightning lightning;
        MrmsOverlays::Chance chance;
        MrmsOverlays::Swath swath;
    };
    auto result = std::make_shared<Loaded>();
    host.background(
        [what, which, result] {
            switch (what) {
                case Kind::Lightning: result->lightning = MrmsOverlays::lightning(which); break;
                case Kind::Chance: result->chance = MrmsOverlays::chance(which); break;
                case Kind::Hail: result->swath = MrmsOverlays::hail(which); break;
                default: result->swath = MrmsOverlays::rotation(which); break;
            }
        },
        [this, &host, result, mine] {
            loading = false;
            if (mine != generation) {
                return;
            }
            switch (kind) {
                case Kind::Lightning:
                    have = result->lightning.ok;
                    strikes = std::move(result->lightning.points);
                    utc = result->lightning.utc;
                    break;
                case Kind::Chance:
                    have = result->chance.ok;
                    lines = std::move(result->chance.lines);
                    chanceCells = std::move(result->chance.cells);
                    break;
                default:
                    have = result->swath.ok;
                    utc = result->swath.utc;
                    swath = std::move(result->swath);
                    break;
            }
            host.redraw();
        });
}

void MrmsOverlayLayer::paintSwath(QPainter& painter, MapHost& host) {
    auto& view = host.view();
    const auto t = view.transform();
    const double ratio = std::max(1.0, painter.device()->devicePixelRatioF());
    const int pixelWidth = std::max(1, static_cast<int>(std::lround(view.pixelWidth() * ratio)));
    const int pixelHeight = std::max(1, static_cast<int>(std::lround(view.pixelHeight() * ratio)));
    // the coarsest level whose cell is no bigger than a pixel of the map: each pixel shows the largest value under it
    const double degreesPerPixel = std::abs(1000.0 / pixelWidth / t.zoom / t.ax);
    size_t level = 0;
    for (size_t i = 0; i < swath.levels.size(); i++) {
        if (swath.levels[i].first.cell <= degreesPerPixel * 1.5) {
            level = i;
        }
    }
    const auto& grid = swath.levels[level].first;
    const auto * cells = reinterpret_cast<const uchar *>(swath.levels[level].second.constData());
    vector<int> columnOf(static_cast<size_t>(pixelWidth));
    for (int i = 0; i < pixelWidth; i++) {
        const double u = (i + 0.5) * 1000.0 / pixelWidth - 500.0;
        const double lon = ((u - t.xPos) / t.zoom - t.bx) / t.ax;
        const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
        columnOf[static_cast<size_t>(i)] = column >= 0 && column < grid.columns ? column : -1;
    }
    QImage image{pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied};
    for (int j = 0; j < pixelHeight; j++) {
        const double v = (j + 0.5) * 1000.0 / pixelWidth - 250.0;
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
            out[i] = column < 0 ? qRgba(0, 0, 0, 0) : swath.colors[source[column]];
        }
    }
    {   // the shapes edged in black, so they read whatever is under them
        const QImage filled = image.copy();
        const int edge = std::max(1, static_cast<int>(std::lround(ratio)));
        for (int j = 0; j < pixelHeight; j++) {
            auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
            const auto * in = reinterpret_cast<const QRgb *>(filled.constScanLine(j));
            for (int i = 0; i < pixelWidth; i++) {
                if (qAlpha(in[i]) == 0) {
                    continue;
                }
                bool border = false;
                for (int d = 1; d <= edge && !border; d++) {
                    border = i - d < 0 || i + d >= pixelWidth || j - d < 0 || j + d >= pixelHeight || qAlpha(in[i - d]) == 0 || qAlpha(in[i + d]) == 0 ||
                        qAlpha(reinterpret_cast<const QRgb *>(filled.constScanLine(j - d))[i]) == 0 || qAlpha(reinterpret_cast<const QRgb *>(filled.constScanLine(j + d))[i]) == 0;
                }
                if (border) {
                    out[i] = qRgba(0, 0, 0, 255);
                }
            }
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRectF{-500.0, -250.0, 1000.0, 1000.0 * pixelHeight / pixelWidth}, image, QRectF{image.rect()});
}

void MrmsOverlayLayer::paint(QPainter& painter, MapHost& host) {
    if (!have) {
        return;
    }
    if (kind == Kind::Hail || kind == Kind::Rotation) {
        if (!swath.levels.empty()) {
            paintSwath(painter, host);
        }
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double perPixel = view.unitsPerPixel();
    const double bottom = -250.0 + 1000.0 * view.pixelHeight() / std::max(1, view.pixelWidth());
    if (kind == Kind::Lightning) {
        const double arm = 2.6 * perPixel;
        for (int age = std::clamp(choice, 1, 4) - 1; age >= 0; age--) {   // the oldest first
            for (const auto& pen : {QPen{QColor{0, 0, 0, 170}, 3.2 * perPixel}, QPen{strikeColors[age], 1.5 * perPixel}}) {
                painter.setPen(pen);
                for (const auto& strike : strikes) {
                    if (strike.age != age) {
                        continue;
                    }
                    const QPointF at = t(strike.lat, strike.lon);
                    if (at.x() < -520.0 || at.x() > 520.0 || at.y() < -270.0 || at.y() > bottom + 20.0) {
                        continue;
                    }
                    painter.drawLine(QPointF{at.x() - arm, at.y()}, QPointF{at.x() + arm, at.y()});
                    painter.drawLine(QPointF{at.x(), at.y() - arm}, QPointF{at.x(), at.y() + arm});
                }
            }
        }
        return;
    }
    // the chance: the bands shaded (one path per band, so touching blocks do not double the shading), then the lines, each with a dark edge under it
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    QPainterPath bands[4];
    for (const auto& block : chanceCells) {
        const int step = block.percent >= 70 ? 3 : block.percent >= 50 ? 2 : block.percent >= 30 ? 1 : 0;
        const double h = block.size / 2.0;
        const QPointF a = t(block.lat + h, block.lon - h), b = t(block.lat - h, block.lon + h);
        if (b.x() < -520.0 || a.x() > 520.0 || b.y() < -270.0 || a.y() > bottom + 20.0) {
            continue;
        }
        bands[step].addRect(QRectF{a, b}.normalized().adjusted(-0.25 * perPixel, -0.25 * perPixel, 0.25 * perPixel, 0.25 * perPixel));
    }
    for (int step = 0; step < 4; step++) {
        QColor fill = chanceColors[step];
        fill.setAlpha(step == 0 ? 60 : 90);
        bands[step].setFillRule(Qt::WindingFill);
        painter.fillPath(bands[step].simplified(), fill);
    }
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const bool edge : {true, false}) {
        for (int step = 0; step < 4; step++) {
            const int level = 10 + 20 * step;
            painter.setPen(edge ? QPen{QColor{0, 0, 0, 200}, 3.4 * perPixel} : QPen{chanceColors[step], 1.8 * perPixel});
            for (const auto& line : lines) {
                if (line.level != level) {
                    continue;
                }
                const QPointF a = t(line.lat1, line.lon1), b = t(line.lat2, line.lon2);
                if ((a.x() < -520.0 && b.x() < -520.0) || (a.x() > 520.0 && b.x() > 520.0) || (a.y() < -270.0 && b.y() < -270.0) || (a.y() > bottom + 20.0 && b.y() > bottom + 20.0)) {
                    continue;
                }
                painter.drawLine(a, b);
            }
        }
    }
}

vector<MapLegendRow> MrmsOverlayLayer::legend() const {
    if (!have) {
        return {};
    }
    MapLegendRow row;
    switch (kind) {
        case Kind::Lightning:
            row.title = "Lightning strikes:";
            for (int age = 0; age < std::clamp(choice, 1, 4); age++) {
                row.entries.push_back({MapLegendEntry::Line, strikeColors[age], strikeNames[age]});
            }
            break;
        case Kind::Chance:
            row.title = choice == 1 ? "Lightning, next 30 min:" : "Lightning, next 60 min:";
            for (int step = 0; step < 4; step++) {
                row.entries.push_back({MapLegendEntry::Line, chanceColors[step], chanceNames[step]});
            }
            break;
        case Kind::Hail:
            row.title = "Hail size (in):";
            for (const auto& band : MrmsOverlays::hailBands()) {
                row.entries.push_back({MapLegendEntry::Square, QColor{band.color.red(), band.color.green(), band.color.blue()}, band.label});
            }
            break;
        default:
            row.title = choice >= 6 ? "Rotation 3-6 km:" : "Rotation 0-2 km:";
            for (const auto& band : MrmsOverlays::rotationBands()) {
                row.entries.push_back({MapLegendEntry::Square, QColor{band.color.red(), band.color.green(), band.color.blue()}, band.label});
            }
            break;
    }
    return {row};
}

QWidget * MrmsOverlayLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{kind == Kind::Chance ? "Looking ahead:" : "Period:", widget});
    auto * combo = UiStandards::combo(widget);
    switch (kind) {
        case Kind::Lightning: combo->addItems({"Last minute", "Last 5 minutes (by age)", "Last 15 minutes (by age)", "Last 30 minutes (by age)"}); break;
        case Kind::Chance: combo->addItems({"Next 30 minutes", "Next 60 minutes"}); break;
        case Kind::Hail: combo->addItems({"Last 30 minutes", "Last hour", "Last 2 hours", "Last 6 hours", "Last 24 hours"}); break;
        default: combo->addItems({"0-2 km: last 30 minutes", "0-2 km: last hour", "0-2 km: last 2 hours", "0-2 km: last 6 hours", "0-2 km: last 24 hours",
                                  "3-6 km: last hour", "3-6 km: last 2 hours", "3-6 km: last 6 hours"}); break;
    }
    combo->setCurrentIndex(std::clamp(choice, 1, combo->count()) - 1);
    layout->addWidget(combo);
    QObject::connect(combo, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        choice = index + 1;
        have = false;
        strikes.clear();
        lines.clear();
        swath = {};
        loading = false;   // one on its way for the old choice is dropped when it arrives
        ++generation;
        reloadNeeded = true;
        changed();
    });
    return widget;
}
