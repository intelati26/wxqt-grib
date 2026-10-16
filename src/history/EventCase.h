// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EVENTCASE_H
#define EVENTCASE_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>
#include <vector>
#include "history/HistoryEvent.h"

// An event's offline copy, its "case": a folder (AppData/archive/cases/<event id>/) that holds what is small and worth keeping of the event, whatever the sources do with it later: the
// SPC outlooks of its convective days (categorical, tornado, wind and hail), the WPC surface analyses every 3 hours (2006 on), the SPC storm reports of each day (the list), and the event
// itself as a file. The screens of the History page read what is kept before they ask the network, so a kept event opens with no connection. Never trimmed by the program: it is removed
// here (forget) or in the Data kept screen.
namespace EventCase {
    struct Item {
        QString name;            // the file in the case folder
        QStringList urls;        // where it can come from, tried in turn (the archive's names changed over the years)
        QString what;            // for the list shown
    };
    struct Summary {
        int files{0};
        qint64 bytes{0};
        int wanted{0};           // what keep() would hold (the files that exist upstream are found when it is asked)
    };
    QString folder(const HistoryEvent&);
    std::vector<Item> items(const HistoryEvent&);
    Summary summary(const HistoryEvent&);
    // fetch what is not there yet (blocking: off the interface thread; several requests at once, behind what is on screen); progress is called with (done, of). Returns the files added.
    int keep(const HistoryEvent&, const std::function<void(int, int)>& progress = {});
    QByteArray read(const HistoryEvent&, const QString& name);   // empty when it is not kept
    void forget(const HistoryEvent&);
    // the names the History page's tabs use for what they show
    QString outlookName(const QDate& issuedDay, const QString& kind);                       // "spc_outlook_20110427_1630_torn.png" (kind: cat, torn, wind, hail)
    QString surfaceName(const QString& kind, const QDateTime& utcThreeHourly);              // "sfc_namussfc_2011042712.gif"
}

#endif  // EVENTCASE_H
