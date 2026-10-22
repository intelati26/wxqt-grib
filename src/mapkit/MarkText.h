// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MARKTEXT_H
#define MARKTEXT_H

#include <algorithm>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include "mapkit/MapLayer.h"

// Text and tags at a place on the map, for the layers: painted at once on the map, or (on a picture of the world in a projection, MapHost::deferMarks) kept to be painted upright on the projected
// picture. Offsets and sizes are in "pixels of the mark" (the layer's `px` is the units one such pixel takes on the map); the painter's pen and font say how the text looks.
namespace MarkText {
    // the size of the painter's font in pixels of the mark
    inline double nominalSize(const QPainter& painter, double px) {
        return painter.font().pixelSize() > 0 ? painter.font().pixelSize() / std::max(1e-9, px) : 11.0;
    }

    // text with its baseline starting at `offset` from the place
    inline void at(MapHost& host, QPainter& painter, double px, double lat, double lon, const QPointF& offset, const QString& text) {
        if (host.deferMarks()) {
            const QFont font = painter.font();
            const QColor color = painter.pen().color();
            const double size = nominalSize(painter, px);
            host.deferMark(lat, lon, [font, color, size, offset, text] (QPainter& p, double unit, double) {
                QFont f{font};
                f.setPixelSize(std::max(6, static_cast<int>(std::lround(size * unit))));
                p.setFont(f);
                p.setPen(color);
                p.drawText(offset * unit, text);
            });
            return;
        }
        painter.drawText(host.view().transform()(lat, lon) + offset * px, text);
    }

    // a label centred on the place with a light edge so that it reads over the colors (the number of a contour line, a high or a low); the painter's font says the size
    inline void halo(MapHost& host, QPainter& painter, double px, double lat, double lon, const QString& text, const QColor& color) {
        const QFont font = painter.font();
        const double size = nominalSize(painter, px);
        const auto draw = [font, size, text, color] (QPainter& p, double unit, const QPointF& place) {
            QFont f{font};
            f.setPixelSize(std::max(6, static_cast<int>(std::lround(size * unit))));
            const QFontMetricsF metrics{f};
            QPainterPath path;
            path.addText(QPointF{place.x() - metrics.horizontalAdvance(text) / 2.0, place.y() + metrics.ascent() / 2.0 - 1.0}, f, text);
            p.setPen(QPen{QColor{255, 255, 255, 220}, 3.0 * unit, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPath(path);
        };
        if (host.deferMarks()) {
            host.deferMark(lat, lon, [draw] (QPainter& p, double unit, double) { draw(p, unit, QPointF{0.0, 0.0}); });
            return;
        }
        draw(painter, px, host.view().transform()(lat, lon));
    }

    // text centred in a square of `half` * 2 pixels round the place
    inline void centred(MapHost& host, QPainter& painter, double px, double lat, double lon, double half, const QString& text) {
        if (host.deferMarks()) {
            const QFont font = painter.font();
            const QColor color = painter.pen().color();
            const double size = nominalSize(painter, px);
            host.deferMark(lat, lon, [font, color, size, half, text] (QPainter& p, double unit, double) {
                QFont f{font};
                f.setPixelSize(std::max(6, static_cast<int>(std::lround(size * unit))));
                p.setFont(f);
                p.setPen(color);
                p.drawText(QRectF{-half * unit, -half * unit, 2.0 * half * unit, 2.0 * half * unit}, Qt::AlignCenter, text);
            });
            return;
        }
        const QPointF at = host.view().transform()(lat, lon);
        painter.drawText(QRectF{at.x() - half * px, at.y() - half * px, 2.0 * half * px, 2.0 * half * px}, Qt::AlignCenter, text);
    }

    // the label of a station: the text on a dark rounded box whose left middle is `offset` from the place (padLeft, padRight round the text)
    inline void tag(MapHost& host, QPainter& painter, double px, double lat, double lon, const QPointF& offset, const QString& text, const QColor& box, const QColor& textColor,
                    double padLeft, double padRight, double radius) {
        const QFont font = painter.font();
        const double size = nominalSize(painter, px);
        const auto draw = [font, size, offset, text, box, textColor, padLeft, padRight, radius] (QPainter& p, double unit, const QPointF& place) {
            QFont f{font};
            f.setPixelSize(std::max(6, static_cast<int>(std::lround(size * unit))));
            p.setFont(f);
            const QFontMetricsF metrics{f};
            const QRectF rect{place.x() + offset.x() * unit, place.y() + offset.y() * unit - metrics.height() / 2.0, metrics.horizontalAdvance(text) + (padLeft + padRight) * unit, metrics.height()};
            p.setPen(Qt::NoPen);
            p.setBrush(box);
            p.drawRoundedRect(rect, radius * unit, radius * unit);
            p.setPen(textColor);
            p.drawText(rect.adjusted(padLeft * unit, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        };
        if (host.deferMarks()) {
            host.deferMark(lat, lon, [draw] (QPainter& p, double unit, double) { draw(p, unit, QPointF{0.0, 0.0}); });
            return;
        }
        draw(painter, px, host.view().transform()(lat, lon));
    }
}

#endif  // MARKTEXT_H
