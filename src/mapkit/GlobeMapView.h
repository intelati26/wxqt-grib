// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GLOBEMAPVIEW_H
#define GLOBEMAPVIEW_H

#include <functional>
#include <string>
#include <vector>
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
    // the polar maps: how far from the pole the sides of the picture reach (degrees of latitude) and whether the picture is a circle (the whole hemisphere to that latitude) or a wide
    // 16:9 frame filled to its corners, as a weather chart of the polar vortex is
    static void setPolarFrame(double reachLatitude, bool wide);
    static bool isPolar(Projection p) { return p == Projection::PolarNorth || p == Projection::PolarSouth; }
    static bool polarWide();
    // lines of latitude and longitude on a picture made by projectWorld / renderWorld (width x height, `scale` pixels per projection unit), labelled where they meet the edge of the map
    static void drawGraticule(QPainter& painter, Projection p, double centerLon, int width, int height, double marks);

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
    // An area shown on the globe as a rectangle with its name (MAG's area picker: hover an area, see where it is); the globe can turn to it, smoothly
    void setPreview(const std::string& name, double west, double south, double east, double north);
    void clearPreview();
    void rotateTo(double lat, double lon, double newZoom = -1.0);   // zoom: -1 keeps it
    // the zoom that shows an area (degrees) whole
    static double zoomFor(double west, double south, double east, double north);
    // With areas set, the area under the pointer is previewed as it moves over the globe and a click chooses it (empty list: off)
    struct Area {
        std::string id;
        double west, south, east, north;
    };
    void setAreas(const std::vector<Area>& list, const std::function<void(const std::string&)>& chosen);
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
    bool previewOn{false};
    std::string previewName;
    double pWest{0}, pSouth{0}, pEast{0}, pNorth{0};
    std::vector<Area> areas;
    std::function<void(const std::string&)> areaChosen;
    std::string areaUnder;
    QTimer turnTimer;
    double fromLat{0}, fromLon{0}, fromZoom{1}, toLat{0}, toLon{0}, toZoom{1};
    int turnStep{0};
};

#endif  // GLOBEMAPVIEW_H
