// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RIVERMAPVIEWER_H
#define RIVERMAPVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QLabel>
#include <QPointF>
#include "radar/NexradWidget.h"
#include "rivers/UtilityRivers.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The NWS river gauges on the same scrollable, zoomable map as the radar screens (state, county and highway lines, cities, your
// location), each a dot in its flood category's colour: click one for its page (the hydrograph, the NWS forecast, the National Water Model
// and the record). The gauge list comes from the NWPS map service (about 13,000 gauges, read once and kept for ten minutes).
class RiverMapViewer : public Window {
public:
    explicit RiverMapViewer(Window * parent);

private:
    void loadGauges();
    void summarize();
    bool shown(const UtilityRivers::Gauge&) const;
    const UtilityRivers::Gauge * gaugeAt(const QPointF& widgetPos) const;
    void paintGauges(QPainter&);
    void paintLegend(QPainter&);
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
    QPointF widgetOf(const UtilityRivers::Gauge&, const Projection2&) const;

    VBox box;
    HBox rowTop;
    ComboBox comboFilter;
    Button buttonRefresh;
    Text textStatus;
    NexradWidget * radar{};
    QLabel * hoverLabel{};
    std::shared_ptr<vector<UtilityRivers::Gauge>> gauges;
    QPointF pointer;
    QPointF pressedAt;
    bool pointerInside{false};
    bool moved{false};
    bool closed{false};
    int generation{0};
};

#endif  // RIVERMAPVIEWER_H
