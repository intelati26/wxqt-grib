// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SURFACEVIEWER_H
#define SURFACEVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QLabel>
#include <QPointF>
#include <QString>
#include <QTimer>
#include "obs/SurfaceStation.h"
#include "radar/MapWidget.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The surface observations of the whole country on the same scrollable, zoomable map as the radar screens: a wind barb, the temperature and the
// dew point at each station, the stations of the airports (the latest METAR of each, from the Aviation Weather Center) and, as an option, the
// state mesonets, RAWS fire-weather stations, road weather, hydrological and other networks that NOAA's MADIS collects (about 30,000 more in a
// day's file). The stations are thinned to one in each patch of the map, so zooming in shows more of them. Hover for a summary, click for
// everything the station reported.
class SurfaceViewer : public Window {
public:
    explicit SurfaceViewer(Window * parent);
    static QString summary(const SurfaceStation&, bool fahrenheit);   // the hover text
    static QString details(const SurfaceStation&);                    // the whole card

private:
    void loadMetars();
    void loadMesonet();
    void rebuild();
    void paintStations(QPainter&);
    void paintLegend(QPainter&);
    int pickAt(const QPointF& widgetPos) const;    // an index into `all`, or -1
    void showHover(const QPointF&);
    void closeEventCustom() override { closed = true; }
    void resizeEventCustom() override;
    void changeZoom(double factor);
    void changePosition(double dx, double dy);
    void fitRadar();
    void showConus();
    bool eventFilter(QObject *, QEvent *) override;
    struct Projection2 {   // the radar projection as x = ax * lon + bx, y = ay * mercator(lat) + by
        double ax;
        double bx;
        double ay;
        double by;
    };
    Projection2 projection() const;
    QColor dotColor(const SurfaceStation&) const;
    double shownTemperature(double celsius) const { return comboUnits.getIndex() == 0 ? celsius * 1.8 + 32.0 : celsius; }

    VBox box;
    HBox rowTop;
    Button buttonRefresh;
    QCheckBox * airportCheck{};
    QCheckBox * mesoCheck{};
    QCheckBox * barbCheck{};
    QCheckBox * valueCheck{};
    ComboBox comboColor;      // flight category / temperature
    ComboBox comboUnits;      // F or C
    Text textStatus;
    MapWidget * radar{};
    QLabel * hoverLabel{};
    QPointF pointer;
    bool pointerInside{false};
    bool closed{false};
    int generation{0};
    int ticks{0};
    QTimer timer;
    vector<SurfaceStation> metars;
    vector<SurfaceStation> mesonet;
    vector<SurfaceStation> all;                 // airports first, then the mesonets by how much they can be trusted
    struct Drawn {
        int index;
        QPointF at;                             // widget pixels
    };
    vector<Drawn> drawn;                        // what the last paint put on the map, for the pointer
    string mesonetError;
    bool mesonetLoading{false};
};

#endif  // SURFACEVIEWER_H
