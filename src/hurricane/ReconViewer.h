// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RECONVIEWER_H
#define RECONVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QImage>
#include <QPlainTextEdit>
#include <QPointF>
#include <QTimer>
#include <QWidget>
#include "hurricane/FloaterGeo.h"
#include "hurricane/HurricaneData.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include <functional>
#include "ui/Window.h"

// One recon flight on its own: the track (coloured by the flight-level or the SFMR surface wind, with barbs), the dropsondes and the vortex fixes, over the storm's GOES floater, and a
// log of what the flight has reported, newest first. It refreshes by hand, or every few minutes while a flight is on.
class ReconMap : public QWidget {
public:
    struct Flight {
        std::vector<UtilityHdob::Ob> obs;           // oldest first
        std::vector<UtilityVdm::Vdm> fixes;
        std::vector<UtilityDropsonde::Drop> drops;
        std::vector<std::pair<double, double>> stormTrack;   // (lat, lon) of the best track, oldest first
        double stormLat{0.0}, stormLon{0.0};
        bool haveStorm{false};
    };
    explicit ReconMap(QWidget * parent = nullptr);
    void setFlight(const Flight& flight);
    void setImage(const QImage& image, const FloaterGeo::Geo& geo);
    void setColoring(bool sfmr) { useSfmr = sfmr; update(); }
    void resetZoom() { zoom = 1.0; pan = {}; update(); }
    void setBarbs(bool on) { barbs = on; update(); }
    void setHours(int h) { hours = h; update(); }   // only the last X hours of the flight before its newest observation are drawn (0 all)
    std::function<void(const UtilityDropsonde::Drop&)> onDrop;   // a click on a dropsonde (its triangle on the map)
    // the colour of a wind in knots (the same scale for the flight level and the surface)
    static QColor windColor(double knots);

private:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    QRectF baseRect() const;                    // the picture fitted to the widget, unzoomed
    QPointF toWidget(double lon, double lat) const;
    QRectF pictureRect() const;
    Flight flight;
    QImage image;
    FloaterGeo::Geo geo;
    bool useSfmr{false};
    bool barbs{true};
    int hours{0};
    long cutNow() const;
    double zoom{1.0};                           // wheel to zoom at the pointer, drag to pan, double click to reset
    QPointF pan;                                // the picture's shift in widget pixels
    QPointF dragFrom, pressAt;
    bool dragging{false};
    const UtilityDropsonde::Drop * dropAt(const QPointF& at) const;   // the dropsonde under that point of the widget, if there is one
};

class ReconViewer : public Window {
public:
    // stormId is the NHC id ("al092026"), name the storm's name ("Isaias")
    ReconViewer(Window * parent, const std::string& stormId, const std::string& name);

private:
    struct Mission {
        std::string key;        // "AF305 0709A ISAIAS"
        std::string label;
        long first{0}, last{0};
        int bulletins{0};
    };
    void refresh(bool forceImage);
    void applyData();
    void chooseMission();
    void rebuild();
    void loadImage();
    void setTimer();
    void closeEventCustom() override { closed = true; }
    static std::string basinOf(const std::string& id);
    VBox box;
    HBox row;
    HBox row2;
    ComboBox comboMission;
    ComboBox comboFloater;
    ComboBox comboColor;
    ComboBox comboRefresh;
    Button buttonRefresh;
    QCheckBox * checkBarbs{};
    Text textStatus;
    ReconMap * map{};
    QPlainTextEdit * log{};
    QTimer timer;
    std::string stormId;
    std::string name;
    std::string selected;       // the mission key chosen ("" until one is)
    std::vector<Mission> missions;
    std::shared_ptr<HurricaneData::ReconData> recon;
    std::shared_ptr<HurricaneData::VdmData> vdm;
    std::shared_ptr<HurricaneData::DropData> drops;
    std::shared_ptr<HurricaneData::StormData> storm;
    QImage image;
    FloaterGeo::Geo geo;
    long imageLoadedAt{0};
    int generation{0};
    bool closed{false};
    bool rebuilding{false};
};

#endif  // RECONVIEWER_H
