// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef INTENSITYVIEWER_H
#define INTENSITYVIEWER_H

#include "ui/ChartExport.h"
#include <map>
#include <memory>
#include <string>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// The storm's intensity against calendar time on one chart pair (maximum wind, minimum pressure): the best track so far, NHC's official forecast, SHIPS
// and LGEM, the median and 25-75 % band of each ensemble (AIFS ENS, IFS ENS, GEFS), the unperturbed AIFS / IFS runs, and the reconnaissance
// observations (strongest flight-level wind and the vortex-message pressures), each family on or off.
class IntensityChart : public QWidget {
public:
    explicit IntensityChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(700, 480); ChartExport::install(this, "Intensity"); }
    void setData(const std::shared_ptr<HurricaneData::StormData>& storm, const std::shared_ptr<HurricaneData::EnsembleData>& ensembles,
                 const std::shared_ptr<HurricaneData::ShipsData>& ships, const std::shared_ptr<HurricaneData::VdmData>& vdm,
                 const std::shared_ptr<HurricaneData::ReconData>& recon);
    void setShown(const string& family, bool shown);

private:
    bool shown(const string& family) const;
    void paintEvent(QPaintEvent *) override;
    std::shared_ptr<HurricaneData::StormData> storm;
    std::shared_ptr<HurricaneData::EnsembleData> ensembles;
    std::shared_ptr<HurricaneData::ShipsData> ships;
    std::shared_ptr<HurricaneData::VdmData> vdm;
    std::shared_ptr<HurricaneData::ReconData> recon;
    std::map<string, bool> hidden;
};

class IntensityViewer : public Window {
public:
    IntensityViewer(Window * parent, const std::shared_ptr<HurricaneData::StormData>& storm, const std::shared_ptr<HurricaneData::EnsembleData>& ensembles,
                    const std::shared_ptr<HurricaneData::ShipsData>& ships, const std::shared_ptr<HurricaneData::VdmData>& vdm,
                    const std::shared_ptr<HurricaneData::ReconData>& recon);

private:
    VBox box;
    HBox rowFamilies;
    IntensityChart * chart{};
};

#endif  // INTENSITYVIEWER_H
