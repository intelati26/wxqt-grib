// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NETSTATUS_H
#define NETSTATUS_H

#include <QDialog>
#include <QLabel>
#include <QPointer>
#include <QTableWidget>
#include <QTimer>

// The list of what the network client (objects/NetManager) has queued and in flight: priority, state, how much is in, how long it has waited and who is waiting on it, with the totals
// (requests made, downloads saved by sharing one, bytes). Opens from a click on any screen's activity line; one window for the whole program.
class NetStatus : public QDialog {
public:
    static void show(QWidget * parent);

private:
    explicit NetStatus(QWidget * parent);
    void refresh();
    QLabel * totals{};
    QTableWidget * table{};
    QTimer timer;
    static QPointer<NetStatus> open;
};

#endif  // NETSTATUS_H
