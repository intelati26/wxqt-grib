// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HISTORYCURSOR_H
#define HISTORYCURSOR_H

#include <QDateTime>
#include <QObject>
#include <QString>

// The time the history screens look at, shared: the History page sets it when an event is opened or the time it opens the screens at is changed; the master map and the Model Viewer
// that were opened at a time follow it, and tell it when the person moves their own time bar, so that screens opened from one event stay on the same moment.
class HistoryCursor : public QObject {
    Q_OBJECT
public:
    static HistoryCursor& get();
    const QDateTime& time() const { return when; }
    const QString& name() const { return label; }
    void set(const QDateTime& utc, const QString& eventName, QObject * origin);   // the page opened an event (or changed its time)
    void setTime(const QDateTime& utc, QObject * origin);                         // a screen moved its time
signals:
    void moved(QObject * origin);                                                  // the time changed: everything but the origin follows

private:
    QDateTime when;
    QString label;
};

#endif  // HISTORYCURSOR_H
