// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ACTIVITYLABEL_H
#define ACTIVITYLABEL_H

#include <QLabel>
#include <QTimer>

// A line for the corner of a screen that says whether the app is working: "Working - 3 downloads in progress, 1 background task" while it is, and the number of downloads
// made since the screen opened when it is not. Look at util/Activity.h for what is counted (the whole app, not only this screen).
class ActivityLabel : public QLabel {
public:
    explicit ActivityLabel(QWidget * parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *) override;

private:
    void update();
    QTimer timer;
    long baseline{0};
};

#endif  // ACTIVITYLABEL_H
