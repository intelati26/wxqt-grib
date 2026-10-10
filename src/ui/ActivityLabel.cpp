// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ActivityLabel.h"
#include <QMouseEvent>
#include "ui/NetStatus.h"
#include "util/Activity.h"

ActivityLabel::ActivityLabel(QWidget * parent) : QLabel{parent}, baseline{Activity::downloadsDone.load()} {
    setStyleSheet("color: gray;");
    setToolTip("What the app is doing: network requests in flight and background jobs running. Click for the list of requests.");
    setCursor(Qt::PointingHandCursor);
    timer.setInterval(250);
    QObject::connect(&timer, &QTimer::timeout, [this] { update(); });
    timer.start();
    update();
}

void ActivityLabel::update() {
    const int downloads = Activity::downloads.load(), tasks = Activity::tasks.load();
    const long done = Activity::downloadsDone.load() - baseline;
    if (downloads == 0 && tasks == 0) {
        setText("Idle  (" + QString::number(done) + " downloads since this screen opened)");
        setStyleSheet("color: gray;");
        return;
    }
    QString text = "Working - ";
    QStringList parts;
    if (downloads > 0) {
        parts << QString::number(downloads) + (downloads == 1 ? " download" : " downloads") + " in progress";
    }
    if (tasks > 0) {
        parts << QString::number(tasks) + (tasks == 1 ? " background task" : " background tasks");
    }
    setText(text + parts.join(", ") + "  (" + QString::number(done) + " done)");
    setStyleSheet("color: #1b6ec2; font-weight: bold;");
}

void ActivityLabel::mousePressEvent(QMouseEvent *) {
    NetStatus::show(window());
}
