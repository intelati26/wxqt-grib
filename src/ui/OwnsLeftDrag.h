// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OWNSLEFTDRAG_H
#define OWNSLEFTDRAG_H

#include <QObject>
#include <QPointer>
#include <QWidget>

// A desktop window's scroll area lets a left-button drag on its content pan the page (ui/ScrolledWindow, a QScroller on the viewport). A widget that is dragged itself (a list whose rows
// are moved, the home screen's layout editor) never gets that drag: the page moves instead, and only a second button held down stopped the scroller. While the pointer is over such
// a widget the scroller lets go of the viewport, and takes it up again when the pointer leaves.
class OwnsLeftDrag : public QObject {
public:
    static void install(QWidget * widget);

protected:
    bool eventFilter(QObject *, QEvent *) override;

private:
    explicit OwnsLeftDrag(QWidget * widget);
    QWidget * viewport() const;   // the viewport of the scroll area the widget is in that has a scroller, or null
    QPointer<QWidget> held;
    bool released{false};
};

#endif  // OWNSLEFTDRAG_H
