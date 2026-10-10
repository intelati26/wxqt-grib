// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ZOOMIMAGE_H
#define ZOOMIMAGE_H

#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>
#include "ui/Window.h"

// A scrollable image view with mouse-wheel zoom and left-drag panning.
// Until the user zooms it behaves like a plain fit-to-viewport image and
// re-fits on resize.
class ZoomImage : public QScrollArea {
    Q_OBJECT

public:
    explicit ZoomImage(Window * parent = nullptr);
    void setBytes(const QByteArray&);
    // swap the image without disturbing the current zoom / pan (for animation)
    void setBytesKeepView(const QByteArray&);
    void fitToViewport();
    bool hasImage() const;
    // "point pick" mode: crosshair cursor + a crosshair marker the caller places
    // via setMarker() at a 0..1 fraction of the source image (for value read-out)
    void setCrosshairMode(bool);
    void setMarker(double fx, double fy);
    void clearMarker();
    // the zoom and place, to put several pictures of the same area on the same view (the comparison tiles): the zoom as a multiple of the fitted size, the centre as fractions of the picture
    struct View {
        bool fitted{true};
        double zoom{1.0}, cx{0.5}, cy{0.5};
    };
    View view() const;
    void setView(const View&);

signals:
    void doubleClicked();
    // fx / fy are the cursor position as a 0..1 fraction of the source image
    void hovered(double fx, double fy);
    void hoverEnded();
    // a left click that did not turn into a drag; fx / fy as for hovered()
    void clicked(double fx, double fy);
    // the user zoomed or panned (not a refit or setView)
    void viewChanged();

protected:
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;

private:
    void applyScale(double newScale, const QPoint& anchor);
    double fitScale() const;
    void repositionMarker();
    void updateRestingCursor();
    QLabel * label;
    QLabel * marker;
    QPixmap source;
    double scale{1.0};
    bool userZoomed{false};
    bool dragging{false};
    bool applying{false};   // a view being put in: not the user's doing
    bool crosshairMode{false};
    bool markerActive{false};
    double markerFx{0.0};
    double markerFy{0.0};
    QPoint dragStart;
    QPoint pressGlobal;
};

#endif  // ZOOMIMAGE_H
