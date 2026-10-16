// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/HistoryCursor.h"

HistoryCursor& HistoryCursor::get() {
    static HistoryCursor * instance = new HistoryCursor;   // lives as long as the program
    return *instance;
}

void HistoryCursor::set(const QDateTime& utc, const QString& eventName, QObject * origin) {
    label = eventName;
    setTime(utc, origin);
}

void HistoryCursor::setTime(const QDateTime& utc, QObject * origin) {
    const auto at = utc.toUTC();
    if (!at.isValid() || at == when) {
        return;
    }
    when = at;
    emit moved(origin);
}
