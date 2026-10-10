// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "radar/MapLegend.h"
#include <algorithm>
#include <cmath>
#include <QFontMetricsF>
#include <QPolygonF>

namespace {
    const double left = -492.0;
    const double usable = 984.0;        // the width the legend may take
    // the sizes below are in screen pixels, times the units per pixel
    const double lineHeightPx = 17.0;
    const double markSpacePx = 26.0;    // the mark, its gap, and the gap after the label
    const double rowGapPx = 22.0;       // between two rows on one line

    double titleWidth(const QFontMetricsF& m, const MapLegendRow& row, double p) {
        return row.title.isEmpty() ? 0.0 : m.horizontalAdvance(row.title) + 10.0 * p;
    }

    double entryWidth(const QFontMetricsF& m, const MapLegendEntry& e, double p) {
        return markSpacePx * p + m.horizontalAdvance(e.label);
    }

    double rowWidth(const QFontMetricsF& m, const MapLegendRow& row, double p) {
        double w = titleWidth(m, row, p);
        for (const auto& e : row.entries) {
            w += entryWidth(m, e, p);
        }
        return w;
    }

    struct Item {
        double x;
        int line;
        const MapLegendRow * row;       // set for a title
        const MapLegendEntry * entry;   // set for an entry
    };
}

double MapLegend::draw(QPainter& painter, const std::vector<MapLegendRow>& rows, double perPixel) {
    if (rows.empty()) {
        return 750.0;
    }
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(12.0 * perPixel))));
    const double p = perPixel;
    const double lineHeight = lineHeightPx * p;
    const double rowGap = rowGapPx * p;
    painter.setFont(font);
    const QFontMetricsF metrics{font};
    // pack: rows that fit beside the one before share its line, the others start a line; entries wrap at the right edge
    std::vector<Item> items;
    int line = 0;
    double x = left;
    bool lineHasRow = false;
    for (const auto& row : rows) {
        if (lineHasRow && x - left + rowGap + rowWidth(metrics, row, p) > usable) {
            line++;
            x = left;
            lineHasRow = false;
        } else if (lineHasRow) {
            x += rowGap;
        }
        items.push_back({x, line, &row, nullptr});
        x += titleWidth(metrics, row, p);
        for (const auto& e : row.entries) {
            const double w = entryWidth(metrics, e, p);
            if (x - left + w > usable && x > left + titleWidth(metrics, row, p) + 1.0) {
                line++;
                x = left + 14.0 * p;   // a wrapped row's next line is indented under its title
            }
            items.push_back({x, line, nullptr, &e});
            x += w;
        }
        lineHasRow = true;
    }
    const int lines = line + 1;
    const double bottom = 748.0;
    const double top = bottom - lines * lineHeight - 2.0 * p;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor{12, 18, 30, 195});
    painter.drawRect(QRectF{-500.0, top, 1000.0, bottom - top + 2.0 * p});
    for (const auto& item : items) {
        const double y = top + 2.0 * p + (item.line + 0.5) * lineHeight;   // the middle of the line
        if (item.row != nullptr) {
            painter.setPen(QColor{235, 235, 235});
            painter.drawText(QPointF{item.x, y + 4.0 * p}, item.row->title);
            continue;
        }
        const auto& e = *item.entry;
        const double mx = item.x;
        painter.setPen(QPen{QColor{255, 255, 255, 200}, 0.9 * perPixel});
        painter.setBrush(e.color);
        switch (e.shape) {
            case MapLegendEntry::Square: painter.drawRect(QRectF{mx, y - 5.0 * p, 10.0 * p, 10.0 * p}); break;
            case MapLegendEntry::Diamond: painter.drawPolygon(QPolygonF{{QPointF{mx + 5.0 * p, y - 6.0 * p}, QPointF{mx + 10.0 * p, y}, QPointF{mx + 5.0 * p, y + 6.0 * p}, QPointF{mx, y}}}); break;
            case MapLegendEntry::Line: painter.setPen(QPen{e.color, 3.0 * perPixel}); painter.drawLine(QPointF{mx, y}, QPointF{mx + 12.0 * p, y}); break;
            default: painter.drawEllipse(QPointF{mx + 5.0 * p, y}, 5.0 * p, 5.0 * p); break;
        }
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QPointF{mx + 15.0 * p, y + 4.0 * p}, e.label);
    }
    return top;
}
