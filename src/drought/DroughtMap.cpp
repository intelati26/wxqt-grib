// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtMap.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QWheelEvent>

namespace {
    const QColor categoryColors[5] = {QColor{"#ffff00"}, QColor{"#fcd37f"}, QColor{"#ffaa00"}, QColor{"#e60000"}, QColor{"#730000"}};
    const char * categoryNames[5] = {"D0 abnormally dry", "D1 moderate drought", "D2 severe drought", "D3 extreme drought", "D4 exceptional drought"};

    QPainterPath pathOf(const std::vector<UtilityDrought::Polygon>& shapes) {
        QPainterPath path;
        path.setFillRule(Qt::OddEvenFill);
        for (const auto& polygon : shapes) {
            for (const auto& ring : polygon) {
                QPolygonF poly;
                for (const auto& [lon, lat] : ring) {
                    poly << QPointF{lon, lat};
                }
                path.addPolygon(poly);
                path.closeSubpath();
            }
        }
        return path;
    }
    QColor changeColor(int moved) {
        switch (std::clamp(moved, -4, 4)) {
            case 1: return QColor{"#fdae6b"};
            case 2: return QColor{"#e6550d"};
            case 3: case 4: return QColor{"#a63603"};
            case -1: return QColor{"#9ecae1"};
            case -2: return QColor{"#3182bd"};
            case -3: case -4: return QColor{"#08306b"};
            default: return QColor{0, 0, 0, 0};
        }
    }
}

DroughtMap::DroughtMap(QWidget * parent) : QWidget{parent} {
    setMinimumSize(520, 380);
    setMouseTracking(true);
}

void DroughtMap::setLand(std::shared_ptr<const std::vector<UtilityDrought::Area>> states) {
    land = std::move(states);
    landPath = QPainterPath{};
    bordersPath = QPainterPath{};
    landPath.setFillRule(Qt::WindingFill);
    if (land) {
        for (const auto& s : *land) {
            const auto p = pathOf(s.shapes);
            landPath.addPath(p);
            bordersPath.addPath(p);
        }
    }
    update();
}

void DroughtMap::setCounties(std::shared_ptr<const std::vector<UtilityDrought::Area>> c) {
    counties = std::move(c);
    countyPath = QPainterPath{};
    if (counties) {
        for (const auto& s : *counties) {
            countyPath.addPath(pathOf(s.shapes));
        }
    }
    update();
}

void DroughtMap::setMonitor(std::shared_ptr<const UtilityDrought::Monitor> m) {
    monitor = std::move(m);
    for (int c = 0; c < 5; c++) {
        std::vector<UtilityDrought::Polygon> here;   // the contiguous states: Alaska, Hawaii and Puerto Rico are drawn far off the map otherwise
        if (monitor) {
            for (const auto& polygon : monitor->shapes[c]) {
                const auto& first = polygon.front();
                if (!first.empty() && first.front().first > -130.0 && first.front().first < -63.0 && first.front().second > 23.0 && first.front().second < 53.0) {
                    here.push_back(polygon);
                }
            }
        }
        categoryPath[c] = pathOf(here);
    }
    update();
}

void DroughtMap::setChange(std::shared_ptr<const ChangeLayer> c) {
    change = std::move(c);
    update();
}

void DroughtMap::setCaption(const QString& text) {
    caption = text;
    update();
}

void DroughtMap::setArea(std::vector<UtilityDrought::Area> a, bool fit) {
    area = std::move(a);
    areaPath = QPainterPath{};
    outsidePath = QPainterPath{};
    if (!area.empty()) {
        for (const auto& s : area) {
            areaPath.addPath(pathOf(s.shapes));
        }
        outsidePath.setFillRule(Qt::OddEvenFill);
        outsidePath.addRect(QRectF{-180.0, -90.0, 360.0, 180.0});
        outsidePath.addPath(areaPath);
    }
    if (fit) {
        fitArea();
    }
    update();
}

void DroughtMap::fitArea() {
    double west = -125.0, east = -66.0, south = 24.0, north = 50.0;
    if (!area.empty()) {
        west = 1e9;
        east = south = -1e9;
        north = -1e9;
        south = 1e9;
        for (const auto& s : area) {
            west = std::min(west, s.west);
            east = std::max(east, s.east);
            south = std::min(south, s.south);
            north = std::max(north, s.north);
        }
    }
    centerLon = (west + east) / 2.0;
    centerLat = (south + north) / 2.0;
    const double k = std::cos(centerLat * M_PI / 180.0);
    const double margin = 1.12;
    pixelsPerDegree = std::min(width() / std::max(0.2, (east - west) * k * margin), height() / std::max(0.2, (north - south) * margin));
    update();
}

QTransform DroughtMap::worldToScreen() const {
    const double k = std::cos(centerLat * M_PI / 180.0);
    QTransform t;
    t.translate(width() / 2.0, height() / 2.0);
    t.scale(pixelsPerDegree * k, -pixelsPerDegree);
    t.translate(-centerLon, -centerLat);
    return t;
}

QPointF DroughtMap::toWorld(const QPointF& screen) const {
    return worldToScreen().inverted().map(screen);
}

void DroughtMap::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{"#dbe7f1"});   // the water
    const auto t = worldToScreen();
    p.save();
    p.setTransform(t);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor{"#f6f4ee"});
    p.drawPath(landPath);
    if (!change) {
        for (int c = 0; c < 5; c++) {   // the milder first, the worse over it
            p.setBrush(categoryColors[c]);
            p.drawPath(categoryPath[c]);
        }
    }
    p.restore();
    if (change && !change->image.isNull()) {
        const QRectF box = t.mapRect(QRectF{change->west, change->north - change->rows * change->step, change->columns * change->step, change->rows * change->step});
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(box, change->image);
    }
    p.save();
    p.setTransform(t);
    QPen border{QColor{60, 60, 60, 190}, 1.0};
    border.setCosmetic(true);
    p.setBrush(Qt::NoBrush);
    if (pixelsPerDegree > 38.0 && !countyPath.isEmpty()) {   // zoomed in: the counties too
        QPen thin{QColor{90, 90, 90, 120}, 0.6};
        thin.setCosmetic(true);
        p.setPen(thin);
        p.drawPath(countyPath);
    }
    p.setPen(border);
    p.drawPath(bordersPath);
    if (!outsidePath.isEmpty()) {   // the rest of the country dimmed, the area outlined
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{245, 245, 245, 150});
        p.drawPath(outsidePath);
        QPen outline{QColor{20, 20, 20}, 2.0};
        outline.setCosmetic(true);
        p.setPen(outline);
        p.setBrush(Qt::NoBrush);
        p.drawPath(areaPath);
    }
    p.restore();
    // the key
    auto font = p.font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    p.setFont(font);
    const int rowHeight = p.fontMetrics().height() + 3;
    std::vector<std::pair<QColor, QString>> keys;
    if (change) {
        keys = {{changeColor(-3), "improved 3 or more categories"}, {changeColor(-2), "improved 2"}, {changeColor(-1), "improved 1"}, {changeColor(1), "worse by 1"},
                {changeColor(2), "worse by 2"}, {changeColor(3), "worse by 3 or more"}};
    } else {
        for (int c = 4; c >= 0; c--) {
            keys.emplace_back(categoryColors[c], categoryNames[c]);
        }
    }
    int keyWidth = 0;
    for (const auto& [color, text] : keys) {
        keyWidth = std::max(keyWidth, p.fontMetrics().horizontalAdvance(text));
    }
    const QRectF keyBox{8.0, height() - 10.0 - rowHeight * static_cast<double>(keys.size()), keyWidth + 40.0, rowHeight * static_cast<double>(keys.size()) + 6.0};
    p.setPen(Qt::NoPen);
    p.setBrush(QColor{255, 255, 255, 215});
    p.drawRoundedRect(keyBox, 4, 4);
    for (size_t i = 0; i < keys.size(); i++) {
        const double y = keyBox.top() + 3.0 + rowHeight * static_cast<double>(i);
        p.setBrush(keys[i].first);
        p.setPen(QColor{80, 80, 80});
        p.drawRect(QRectF{keyBox.left() + 6.0, y + 2.0, 18.0, rowHeight - 5.0});
        p.drawText(QPointF{keyBox.left() + 30.0, y + rowHeight - 5.0}, keys[i].second);
    }
    if (!caption.isEmpty()) {
        p.setPen(QColor{30, 30, 30});
        const QRectF box{8.0, 6.0, width() - 16.0, rowHeight * 2.0};
        p.drawText(box, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, caption);
    }
}

QString DroughtMap::readout(const QPointF& w) const {
    QString text = QString::number(std::abs(w.y()), 'f', 2) + (w.y() >= 0 ? "° N  " : "° S  ") + QString::number(std::abs(w.x()), 'f', 2) + (w.x() >= 0 ? "° E" : "° W");
    if (change && change->columns > 0) {
        const int x = static_cast<int>(std::floor((w.x() - change->west) / change->step)), y = static_cast<int>(std::floor((change->north - w.y()) / change->step));
        if (x >= 0 && y >= 0 && x < change->columns && y < change->rows) {
            const int moved = change->moved[static_cast<size_t>(y) * static_cast<size_t>(change->columns) + static_cast<size_t>(x)];
            text += moved == 0 ? "\nno change" : moved > 0 ? QString{"\nworse by %1 categor%2"}.arg(moved).arg(moved == 1 ? "y" : "ies") : QString{"\nimproved by %1 categor%2"}.arg(-moved).arg(moved == -1 ? "y" : "ies");
        }
    } else if (monitor) {
        int found = -1;
        for (int c = 4; c >= 0 && found < 0; c--) {
            if (categoryPath[c].contains(w)) {
                found = c;
            }
        }
        text += found < 0 ? "\nno drought" : QString{"\n"} + categoryNames[found];
    }
    if (land) {
        for (const auto& s : *land) {
            if (w.x() >= s.west && w.x() <= s.east && w.y() >= s.south && w.y() <= s.north && pathOf(s.shapes).contains(w)) {
                text = QString::fromStdString(s.name) + "\n" + text;
                break;
            }
        }
    }
    return text;
}

void DroughtMap::wheelEvent(QWheelEvent * event) {
    const auto before = toWorld(event->position());
    pixelsPerDegree = std::clamp(pixelsPerDegree * std::pow(1.0015, event->angleDelta().y()), 4.0, 2500.0);
    const auto after = toWorld(event->position());   // keep the point under the pointer where it is
    centerLon += before.x() - after.x();
    centerLat += before.y() - after.y();
    update();
}

void DroughtMap::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragFrom = event->position();
    }
}

void DroughtMap::mouseMoveEvent(QMouseEvent * event) {
    if (dragging) {
        const auto a = toWorld(dragFrom), b = toWorld(event->position());
        centerLon += a.x() - b.x();
        centerLat += a.y() - b.y();
        dragFrom = event->position();
        update();
        return;
    }
    QToolTip::showText(event->globalPosition().toPoint(), readout(toWorld(event->position())), this);
}

void DroughtMap::mouseReleaseEvent(QMouseEvent *) {
    dragging = false;
}

void DroughtMap::mouseDoubleClickEvent(QMouseEvent *) {
    fitArea();
}
