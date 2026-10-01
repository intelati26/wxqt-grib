// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MIDDLEDRAGSCROLL_H
#define MIDDLEDRAGSCROLL_H

#include <QObject>
#include <QPoint>
#include <QScrollArea>

// Hold the middle mouse button anywhere on a scroll area's content and drag: the page follows the pointer (both
// directions), like panning a map. Left-drag and the wheel keep working as before. A middle press on a child widget
// is taken over (nothing underneath sees it), so e.g. a thumbnail is not "clicked" by the gesture.
class MiddleDragScroll : public QObject {
public:
    explicit MiddleDragScroll(QScrollArea * area);
    ~MiddleDragScroll() override;

protected:
    bool eventFilter(QObject *, QEvent *) override;

private:
    bool inside(QObject *) const;
    void finish();
    QScrollArea * area;
    bool dragging{false};
    QPoint startPos;
    int startH{0};
    int startV{0};
};

#endif  // MIDDLEDRAGSCROLL_H
