// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MarkText.h"
#include "mapkit/BandLines.h"
#include <algorithm>
#include <cmath>
#include <QPainterPath>
#include "radar/MapView.h"

std::vector<BandLines::Contour> BandLines::make(const GfsGrid::Grid& grid, const std::vector<std::pair<double, QString>>& levels) {
    std::vector<Contour> out;
    if (grid.empty()) {
        return out;
    }
    const double west = grid.lon0, east = grid.lon0 + (grid.columns - 1) * grid.step, north = grid.lat0, south = grid.lat0 - (grid.rows - 1) * grid.step;
    for (const auto& [level, label] : levels) {
        Contour c;
        c.label = label;
        c.lines = GfsGrid::contour(grid, level, west, south, east, north);
        if (!c.lines.empty()) {
            out.push_back(std::move(c));
        }
    }
    return out;
}

void BandLines::paint(QPainter& painter, MapHost& host, const std::vector<Contour>& contours) {
    if (contours.empty()) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double px = view.unitsPerPixel();
    QFont font{painter.font()};
    font.setBold(true);
    font.setPixelSize(static_cast<int>(10.5 * px));
    painter.save();
    painter.setFont(font);
    painter.setBrush(Qt::NoBrush);
    for (const auto& contour : contours) {
        for (const auto& line : contour.lines) {
            if (line.size() < 4) {
                continue;
            }
            QPainterPath path;
            for (size_t i = 0; i < line.size(); i++) {
                const auto at = t(line[i].lat, line[i].lon);
                i == 0 ? path.moveTo(at) : path.lineTo(at);
            }
            painter.setPen(QPen{QColor{30, 30, 40, 150}, 0.9 * px});
            painter.drawPath(path);
            const double length = path.length() / std::max(1e-9, px);   // screen pixels
            if (length < 90.0) {
                continue;
            }
            const std::vector<double> places = length < 350.0 ? std::vector<double>{0.5} : std::vector<double>{0.25, 0.75};   // a number in the middle of a short line, two on a long one
            for (const double where : places) {
                const QPointF mid = path.pointAtPercent(where);
                const QPointF pixels{(mid.x() + 500.0) / px, (mid.y() + 250.0) / px};
                if (!host.claimCell(pixels, 60.0)) {
                    continue;
                }
                const double width = (contour.label.size() * 6.6 + 6.0) * px;
                const auto [labelLat, labelLon] = host.view().toLatLon(pixels);
                MarkText::tag(host, painter, px, labelLat, labelLon, QPointF{-width / px / 2.0, 0.0}, contour.label, QColor{255, 255, 255, 215}, QColor{25, 25, 35}, 3.0, 3.0, 3.0);
                painter.setBrush(Qt::NoBrush);
            }
        }
    }
    painter.restore();
}
