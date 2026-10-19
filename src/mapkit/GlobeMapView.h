// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GLOBEMAPVIEW_H
#define GLOBEMAPVIEW_H

#include <functional>
#include <QImage>
#include <QTimer>
#include <QWidget>
#include "mapkit/MapLayer.h"
#include "radar/MapView.h"

// The master map on a globe: every layer that is on is painted as it is on the flat map, into a picture of the part of the world in view (a skin: Mercator, as the layers draw it), and the picture
// is put on a sphere seen from space, so nothing is stretched toward the poles. Turn it with the mouse, zoom with the wheel; the skin is made again for the part in view when the turning or
// zooming stops and when a layer has new data. Hover and click ask the layers about the place under the pointer as the flat map does.
class GlobeMapView : public QWidget {
public:
    explicit GlobeMapView(QWidget * parent);
    // The world projections of the picture the export makes of the whole world (the screen shows the globe only): Robinson, the compromise of the United Nations' world maps; Equal Earth and
    // Mollweide, which keep areas true
    enum class Projection { Globe, Robinson, EqualEarth, Mollweide, PolarNorth, PolarSouth };   // Robinson: the compromise projection of the United Nations' world maps; Equal Earth and Mollweide keep areas; PolarNorth and PolarSouth: the stereographic projection from the pole to 20 degrees (the polar vortex; the longitude of the centre is the meridian that points down)
    static const char * projectionName(Projection p);
    static bool inverse(Projection p, double x, double y, double& lat, double& lon);   // projection units -> degrees (false outside the shape)
    static bool forward(Projection p, double lat, double lon, double& x, double& y);   // degrees -> projection units (lon relative to the center)
    static void extent(Projection p, double& halfWidth, double& halfHeight);

    struct Skin {
        QImage image;
        MapView::Transform transform{};
        MapView::SavedState state{};
        int width{0}, height{0};
        double west{0}, south{0}, east{0}, north{0};
        bool valid() const { return !image.isNull(); }
    };
    // the world as one picture of `width` pixels in an equal-area projection, from a skin of the whole world centred on `centerLon` (transparent outside the shape)
    static QImage projectWorld(const Skin& skin, Projection p, double centerLon, int width);
    // the picture of a region (degrees) at a width in pixels, painted by the layers; the legend rows for under the globe
    // (referencePixelsPerDegree: the scale the person sees the globe at, so that marks are not thinned for a picture finer than the screen)
    std::function<Skin(double minLat, double maxLat, double minLon, double maxLon, int width, double referencePixelsPerDegree)> makeSkin;
    std::function<MapHit(const Skin&, double lat, double lon)> pick;
    std::function<void(QPainter&, int width, int height)> paintLegend;

    void setCenter(double lat, double lon) { lat0 = lat; lon0 = lon; scheduleSkin(); update(); }
    void scheduleSkin(int milliseconds = 350);
    double centerLat() const { return lat0; }
    double centerLon() const { return lon0; }
    // a place under a pixel of the view; false off the globe
    bool place(const QPointF& pixel, double& lat, double& lon) const;

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    double radius() const;
    QPointF center() const;
    void makeNow();
    double lat0{35.0}, lon0{-95.0}, zoom{1.0};
    Skin skin;
    QTimer timer;
    QPointF last;
    bool dragging{false};
    bool moved{false};
    MapHit hover;
};

#endif  // GLOBEMAPVIEW_H
