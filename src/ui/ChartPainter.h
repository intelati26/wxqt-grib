// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTPAINTER_H
#define CHARTPAINTER_H

#include <QFontMetricsF>
#include <QPaintDevice>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <vector>

// A QPainter for the program's charts that keeps their text readable: a label that would run off the edge of the widget is
// moved back inside (or shortened with "..." when it is wider than the widget), and a short label (an axis tick, a month, a
// value) that would land on one already drawn is left out rather than printed over it. Long texts (titles, legends, notes) are
// always drawn. The charts call drawText as before; only the painter's type changes. Rotated text is drawn as asked.
class ChartPainter : public QPainter {
public:
    using QPainter::QPainter;
    using QPainter::drawText;

    void drawText(const QRectF& box, int flags, const QString& text, QRectF * bounding = nullptr) {
        if (!plain() || text.isEmpty() || (flags & (Qt::TextWordWrap | Qt::TextWrapAnywhere)) || text.contains('\n')) {
            QPainter::drawText(wrapped(box, flags, text), flags, text, bounding);
            return;
        }
        const QFontMetricsF fm{font()};
        const QRectF limit = bounds();
        QString shown = text;
        double w = fm.horizontalAdvance(shown);
        if (w > limit.width()) {
            shown = fm.elidedText(shown, Qt::ElideRight, limit.width());
            w = fm.horizontalAdvance(shown);
        }
        const double h = fm.height();
        QRectF at = box;
        if (flags & Qt::AlignRight) {
            at.setLeft(std::min(box.left(), box.right() - w));
        } else if (flags & Qt::AlignHCenter) {
            const double extra = std::max(0.0, w - box.width());
            at.adjust(-extra / 2.0, 0, extra / 2.0, 0);
        } else {
            at.setRight(std::max(box.right(), box.left() + w));
        }
        at = inside(at, limit);
        if (at.height() < h) {   // a box the text does not fit in vertically: keep its centre
            const double extra = h - at.height();
            at.adjust(0, -extra / 2.0, 0, extra / 2.0);
            at = inside(at, limit);
        }
        const QRectF ink = textRect(at, flags, w, h);
        if (shown.size() <= 8 && collides(ink)) {
            return;
        }
        remember(ink);
        QPainter::drawText(at, flags, shown, bounding);
    }

    void drawText(const QRect& box, int flags, const QString& text, QRect * bounding = nullptr) {
        QRectF found;
        drawText(QRectF{box}, flags, text, &found);
        if (bounding) {
            *bounding = found.toRect();
        }
    }

    // a baseline position: the text starts there
    void drawText(const QPointF& at, const QString& text) {
        if (!plain() || text.isEmpty() || text.contains('\n')) {
            QPainter::drawText(at, text);
            return;
        }
        const QFontMetricsF fm{font()};
        const QRectF limit = bounds();
        QString shown = text;
        double w = fm.horizontalAdvance(shown);
        double x = at.x();
        if (x + w > limit.right()) {
            x = std::max(limit.left(), limit.right() - w);   // the same label, moved left until it is inside
        }
        if (w > limit.right() - x) {
            shown = fm.elidedText(shown, Qt::ElideRight, limit.right() - x);
            w = fm.horizontalAdvance(shown);
        }
        const double y = std::clamp(at.y(), limit.top() + fm.ascent(), limit.bottom() - fm.descent());
        const QRectF ink{x, y - fm.ascent(), w, fm.height()};
        if (shown.size() <= 8 && collides(ink)) {
            return;
        }
        remember(ink);
        QPainter::drawText(QPointF{x, y}, shown);
    }

    void drawText(int x, int y, const QString& text) {
        drawText(QPointF{static_cast<double>(x), static_cast<double>(y)}, text);
    }

    void drawText(const QPoint& at, const QString& text) {
        drawText(QPointF{at}, text);
    }

private:
    // a painter that is only moved or scaled: its logical rectangle maps straight onto the widget
    bool plain() const {
        return worldTransform().type() <= QTransform::TxScale && device() != nullptr && !hasClipping();
    }

    QRectF bounds() const {
        const QRectF full{0, 0, static_cast<double>(device()->width()), static_cast<double>(device()->height())};
        return worldTransform().inverted().mapRect(full).adjusted(2, 1, -2, -1);
    }

    static QRectF inside(QRectF r, const QRectF& limit) {
        if (r.width() > limit.width()) {
            r.setWidth(limit.width());
        }
        if (r.height() > limit.height()) {
            r.setHeight(limit.height());
        }
        if (r.left() < limit.left()) {
            r.moveLeft(limit.left());
        }
        if (r.right() > limit.right()) {
            r.moveRight(limit.right());
        }
        if (r.top() < limit.top()) {
            r.moveTop(limit.top());
        }
        if (r.bottom() > limit.bottom()) {
            r.moveBottom(limit.bottom());
        }
        return r;
    }

    // the box with a wrapped text: moved inside the widget, no other change
    QRectF wrapped(const QRectF& box, int, const QString&) const {
        return plain() ? inside(box, bounds()) : box;
    }

    // where the glyphs of a single line end up inside `at`
    static QRectF textRect(const QRectF& at, int flags, double w, double h) {
        double x = at.left();
        if (flags & Qt::AlignRight) {
            x = at.right() - w;
        } else if (flags & Qt::AlignHCenter) {
            x = at.center().x() - w / 2.0;
        }
        double y = at.top();
        if (flags & Qt::AlignBottom) {
            y = at.bottom() - h;
        } else if (flags & Qt::AlignVCenter) {
            y = at.center().y() - h / 2.0;
        }
        return QRectF{x, y, w, h};
    }

    bool collides(const QRectF& r) const {
        const QRectF grown = worldTransform().mapRect(r).adjusted(-3, 0, 3, 0);
        for (const auto& other : drawn) {
            if (grown.intersects(other)) {
                return true;
            }
        }
        return false;
    }

    void remember(const QRectF& r) {
        drawn.push_back(worldTransform().mapRect(r));
    }

    std::vector<QRectF> drawn;
};

#endif  // CHARTPAINTER_H
