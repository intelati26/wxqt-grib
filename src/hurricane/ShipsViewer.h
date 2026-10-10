// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SHIPSVIEWER_H
#define SHIPSVIEWER_H

#include "ui/ChartExport.h"
#include <memory>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// NHC's SHIPS forecast for a storm on one sheet: the rapid-intensification (RI) probabilities as a table (how many times the usual chance), a small
// map of the forecast track, and strips against forecast hour of the maximum wind (SHIPS with and without land, LGEM, NHC's forecast), the maximum
// potential intensity, vertical wind shear, sea surface temperature, mid-level humidity and ocean heat content.
class ShipsChart : public QWidget {
public:
    explicit ShipsChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(760, 520); ChartExport::install(this, "SHIPS forecast"); }
    void setData(const std::shared_ptr<HurricaneData::ShipsData>& ships, const std::shared_ptr<HurricaneData::StormData>& storm);
    static QString summary(const UtilityShips::Ships&);   // one line: peak intensity, shear, SST, RI

private:
    void paintEvent(QPaintEvent *) override;
    std::shared_ptr<HurricaneData::ShipsData> ships;
    std::shared_ptr<HurricaneData::StormData> storm;
};

class ShipsViewer : public Window {
public:
    ShipsViewer(Window * parent, const std::shared_ptr<HurricaneData::ShipsData>& ships, const std::shared_ptr<HurricaneData::StormData>& storm);

private:
    VBox box;
    Text textStatus;
    ShipsChart * chart{};
};

#endif  // SHIPSVIEWER_H
