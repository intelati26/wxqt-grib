// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ZOOMIMAGE_H
#define ZOOMIMAGE_H

#include <functional>
#include <QLabel>
#include <QPixmap>
#include <QPoint>
#include <QScrollArea>
#include <QString>
#include <vector>
class QPushButton;
#include "ui/Window.h"

// A scrollable image view with mouse-wheel zoom and left-drag panning.
// Until the user zooms it behaves like a plain fit-to-viewport image and
// re-fits on resize.
class ZoomImage : public QScrollArea {
    Q_OBJECT

public:
    explicit ZoomImage(Window * parent = nullptr);
    void setBytes(const QByteArray&);
    // a right-click "Save image..." for the picture shown, as every other screen's pictures have (on by default; a screen with its own save of the picture turns it off)
    void setSaveMenu(bool on) { saveMenu = on; }
    void setSaveLoopHandler(std::function<void()> handler) { saveLoopHandler = std::move(handler); }   // the screen's own save (its loop, its names and headers): the last item of the right-click menu
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
    // small round buttons over the picture, each at a 0..1 fraction of the source image (the stations of a map). They are widgets laid over the view, not part of the picture,
    // so nothing saved or exported from the picture has them; where they would crowd each other at the zoom shown, only the first of them is shown.
    struct Pin {
        double fx{0.0}, fy{0.0};
        QString id;
        QString tip;
    };
    void setPins(const std::vector<Pin>&);
    void clearPins();
    // any widget laid over the picture at a fraction of it, centered there (the glyphs of a map); the picture owns them. The same as the pins, they are not part of the picture.
    void addOverlay(QWidget *, double fx, double fy);
    void moveOverlay(QWidget *, double fx, double fy);
    void removeOverlay(QWidget *);
    void clearOverlays();
    void setOverlayClip(const QRectF& fractions);   // the overlays are cut to this part of the picture (a map's frame, so they do not cover its title)
    QRectF visibleFraction() const;   // the part of the picture in view, as fractions of it
    QSize pictureSize() const;        // the picture as shown (pixels)

signals:
    void doubleClicked();
    // fx / fy are the cursor position as a 0..1 fraction of the source image
    void hovered(double fx, double fy);
    void hoverEnded();
    // a left click that did not turn into a drag; fx / fy as for hovered()
    void clicked(double fx, double fy);
    // the user zoomed or panned (not a refit or setView)
    void viewChanged();
    void pinClicked(const QString& id);

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
    void repositionPins();
    std::vector<Pin> pins;
    std::vector<QPushButton *> pinButtons;
    struct Overlay {
        QWidget * widget;
        double fx, fy;
    };
    std::vector<Overlay> overlays;
    QRectF overlayClip{0.0, 0.0, 1.0, 1.0};
    void updateRestingCursor();
    QLabel * label;
    QLabel * marker;
    QPixmap source;
    QByteArray sourceBytes;   // the picture as it came (what a save writes)
    bool saveMenu{true};
    std::function<void()> saveLoopHandler;
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
