// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HOVERTIP_H
#define HOVERTIP_H

#include <QPointer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QToolTip>
#include <QWidget>

// The read-out of the point under the pointer on a chart or a map, shown the way a tooltip is: after the pointer has rested a moment, where it rested, and gone when it moves off. (Calling
// QToolTip::showText on every move showed it at once and had it follow the pointer.)
namespace HoverTip {
    inline void show(QWidget * widget, const QPoint& global, const QString& text, int delayMs = 450, int slack = 6) {
        static QTimer timer;
        static QPointer<QWidget> where;
        static QPoint at;
        static QString what;
        static int radius = 6;
        static bool wired = false;
        if (!wired) {
            wired = true;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, [] {
                if (where) {
                    const auto local = where->mapFromGlobal(at);
                    QToolTip::showText(at, what, where, QRect{local - QPoint{radius, radius}, QSize{2 * radius, 2 * radius}});
                }
            });
        }
        QToolTip::hideText();
        radius = slack;
        where = widget;
        at = global;
        what = text;
        timer.start(delayMs);
    }
    inline void hide() {
        QToolTip::hideText();
    }
}

#endif  // HOVERTIP_H
