// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HURRICANEVIEWER_H
#define HURRICANEVIEWER_H

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <QCheckBox>
#include <QLabel>
#include <QPointF>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "radar/MapView.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The Atlantic hurricane screen: the storm picked from NHC's list on a map of the basin with its best track, the official forecast and the
// spaghetti of the model guidance (ATCF aid_public, newest run of each model, grouped so a family can be switched on or off), and the
// aircraft reconnaissance flight tracks (HDOB bulletins) coloured by flight-level or surface wind. Hover for the model / the observation;
// the side panel carries the storm's numbers and the recon summary.
class HurricaneViewer : public Window {
public:
    explicit HurricaneViewer(Window * parent);

private:
    void loadList();
    void loadStorm();
    void loadRecon();
    void loadEnsembles();
    void loadShips();
    void loadPod();
    void loadVdm();
    void showStorm();                       // after a storm has loaded: the panel text, the map region
    void zoomToStorm();
    void updateInfo();
    void paintMap(QPainter&);
    void paintPlannedRecon(QPainter&);
    void paintLegend(QPainter&);
    void showHover(const QPointF& pixels);
    bool groupShown(UtilityAtcf::Group) const;
    bool reconNear(const UtilityHdob::Ob&) const;
    void closeEventCustom() override { closed = true; }
    void resizeEventCustom() override;
    QColor reconColor(const UtilityHdob::Ob&) const;
    static QColor groupColor(UtilityAtcf::Group);
    static QColor categoryColor(int category);

    struct Hit {
        QString text;
        double distance;
    };

    VBox box;
    HBox rowTop;
    HBox rowMain;
    ComboBox comboStorm;
    Button buttonRefresh;
    Button buttonZoom;
    Text textStatus;
    std::unique_ptr<MapView> view;
    QWidget * panel{};
    QLabel * infoLabel{};
    QLabel * hoverLabel{};
    std::vector<std::pair<UtilityAtcf::Group, QCheckBox *>> groupChecks;
    QCheckBox * reconCheck{};
    QCheckBox * coneCheck{};                                  // the forecast cone
    QCheckBox * radiiCheck{};
    QCheckBox * fixCheck{};                                   // the recon centre fixes (vortex messages)
    QCheckBox * podCheck{};                                   // the planned recon flights                                 // the wind radii now
    QCheckBox * ensembleChecks[3]{};                          // AIFS ENS members, IFS ENS members, the unperturbed runs
    Button buttonStats;
    Button buttonShips;
    Button buttonPod;
    Button buttonVdm;
    Button buttonIntensity;
    ComboBox comboRecon;                                      // what colours the flight tracks
    std::vector<vector<std::pair<float, float>>> coast;       // the basin's coastlines and borders (lon, lat)
    vector<HurricaneData::StormEntry> entries;
    std::shared_ptr<HurricaneData::StormData> storm;
    std::shared_ptr<HurricaneData::ReconData> recon;
    std::shared_ptr<HurricaneData::EnsembleData> ensembles;
    std::shared_ptr<HurricaneData::ShipsData> ships;
    std::shared_ptr<HurricaneData::PodData> pod;
    std::shared_ptr<HurricaneData::VdmData> vdm;
    string hoverTech;                                         // the guidance line under the pointer, drawn heavier
    int generation{0};
    bool closed{false};
    bool filling{false};                                      // the combo is being filled: its change is not a pick
};

#endif  // HURRICANEVIEWER_H
