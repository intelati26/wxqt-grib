// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/MiddleDragScroll.h"
#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWidget>

MiddleDragScroll::MiddleDragScroll(QScrollArea * area)
    : QObject{area}
    , area{area}
{
    qApp->installEventFilter(this);   // children get their mouse events first, so watch them all
}

MiddleDragScroll::~MiddleDragScroll() {
    finish();
}

bool MiddleDragScroll::inside(QObject * object) const {
    const auto * widget = qobject_cast<QWidget *>(object);
    return widget != nullptr && (widget == area->viewport() || area->viewport()->isAncestorOf(widget));
}

void MiddleDragScroll::finish() {
    if (dragging) {
        dragging = false;
        QGuiApplication::restoreOverrideCursor();
    }
}

bool MiddleDragScroll::eventFilter(QObject * object, QEvent * event) {
    const auto type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseMove && type != QEvent::MouseButtonRelease) {
        return false;
    }
    auto * mouse = static_cast<QMouseEvent *>(event);
    if (type == QEvent::MouseButtonPress) {
        if (mouse->button() == Qt::MiddleButton && inside(object)) {
            dragging = true;
            startPos = mouse->globalPosition().toPoint();
            startH = area->horizontalScrollBar()->value();
            startV = area->verticalScrollBar()->value();
            QGuiApplication::setOverrideCursor(Qt::SizeAllCursor);
            return true;
        }
        return false;
    }
    if (!dragging) {
        return false;
    }
    if (type == QEvent::MouseMove) {
        if (!(mouse->buttons() & Qt::MiddleButton)) {   // released somewhere we did not see
            finish();
            return false;
        }
        const auto delta = mouse->globalPosition().toPoint() - startPos;
        area->horizontalScrollBar()->setValue(startH - delta.x());
        area->verticalScrollBar()->setValue(startV - delta.y());
        return true;
    }
    if (mouse->button() == Qt::MiddleButton) {   // release
        finish();
        return true;
    }
    return false;
}
