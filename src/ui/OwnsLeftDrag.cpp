// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/OwnsLeftDrag.h"
#include <QEvent>
#include <QScrollArea>
#include <QScroller>

OwnsLeftDrag::OwnsLeftDrag(QWidget * widget) : QObject{widget} {
    widget->installEventFilter(this);
}

void OwnsLeftDrag::install(QWidget * widget) {
    new OwnsLeftDrag{widget};   // owned by the widget
}

QWidget * OwnsLeftDrag::viewport() const {
    for (auto * w = qobject_cast<QWidget *>(parent()); w != nullptr; w = w->parentWidget()) {
        if (const auto * area = qobject_cast<QScrollArea *>(w); area != nullptr && QScroller::hasScroller(area->viewport())) {
            return area->viewport();
        }
    }
    return nullptr;
}

bool OwnsLeftDrag::eventFilter(QObject *, QEvent * event) {
    if (event->type() == QEvent::Enter && !released) {
        if (auto * view = viewport()) {
            held = view;
            released = true;
            QScroller::ungrabGesture(view);
        }
    } else if (event->type() == QEvent::Leave && released) {
        released = false;
        if (held) {
            QScroller::grabGesture(held, QScroller::LeftMouseButtonGesture);
        }
    }
    return false;
}
