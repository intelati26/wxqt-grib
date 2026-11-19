// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QCheckBox>
#include <QDateTimeEdit>
#include <QTimeZone>

// The calls that Qt 6.7 replaced (so that the older Qt the source can still be built with, Debian bookworm's 6.4, and the newer one that has dropped the old call both take them): in one place
// instead of a version check at each use. When 6.4 is no longer built with, the bodies below are the whole change to make.
namespace QtCompat {
    // a date and time box that reads and shows UTC
    inline void utc(QDateTimeEdit * edit) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        edit->setTimeZone(QTimeZone::utc());
#else
        edit->setTimeSpec(Qt::UTC);
#endif
    }
    // called when the box is ticked or unticked (the lambda takes no argument)
    template <typename F>
    inline void toggled(QCheckBox * box, QObject * context, F&& call) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(box, &QCheckBox::checkStateChanged, context, [call] (Qt::CheckState) { call(); });
#else
        QObject::connect(box, &QCheckBox::stateChanged, context, [call] (int) { call(); });
#endif
    }
}

#endif  // QTCOMPAT_H
