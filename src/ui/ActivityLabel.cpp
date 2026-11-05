// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ActivityLabel.h"
#include <QMouseEvent>
#include <QPalette>
#include "ui/NetStatus.h"
#include "util/Activity.h"

ActivityLabel::ActivityLabel(QWidget * parent) : QLabel{parent}, baseline{Activity::downloadsDone.load()}, baselineIndexes{Activity::indexesDone.load()} {
    setForegroundRole(QPalette::PlaceholderText);   // the theme's dimmed color
    setToolTip("What the app is doing: network requests in flight and background jobs running. Click for the list of requests.");
    setCursor(Qt::PointingHandCursor);
    timer.setInterval(250);
    QObject::connect(&timer, &QTimer::timeout, [this] { update(); });
    timer.start();
    update();
}

void ActivityLabel::update() {
    const int downloads = Activity::downloads.load(), tasks = Activity::tasks.load();
    const int indexes = Activity::indexes.load();
    const long indexDone = Activity::indexesDone.load() - baselineIndexes;
    const long done = Activity::downloadsDone.load() - baseline - indexDone;   // data files; the index (.idx) files are counted apart
    const QString indexText = indexDone > 0 ? ", " + QString::number(indexDone) + " index files" : QString{};
    if (const auto note = Activity::note(); !note.empty()) {   // a long job says what it is doing
        setText("Working - " + QString::fromStdString(note));
        setStyleSheet("font-weight: bold;");
        setForegroundRole(QPalette::Link);
        return;
    }
    if (downloads == 0 && tasks == 0) {
        setText("Idle  (" + QString::number(done) + " downloads" + indexText + " since this screen opened)");
        setStyleSheet("");
        setForegroundRole(QPalette::PlaceholderText);
        return;
    }
    QString text = "Working - ";
    QStringList parts;
    if (downloads - indexes > 0) {
        parts << QString::number(downloads - indexes) + (downloads - indexes == 1 ? " download" : " downloads") + " in progress";
    }
    if (indexes > 0) {
        parts << QString::number(indexes) + (indexes == 1 ? " index file" : " index files") + " in progress";
    }
    if (tasks > 0) {
        parts << QString::number(tasks) + (tasks == 1 ? " background task" : " background tasks");
    }
    setText(text + parts.join(", ") + "  (" + QString::number(done) + " done" + indexText + ")");
    setStyleSheet("font-weight: bold;");
    setForegroundRole(QPalette::Link);
}

void ActivityLabel::mousePressEvent(QMouseEvent *) {
    NetStatus::show(window());
}
