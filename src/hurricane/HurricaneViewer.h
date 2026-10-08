// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HURRICANEVIEWER_H
#define HURRICANEVIEWER_H

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <QCheckBox>
#include <QTimer>
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
    // basin "al" / "ep" / "cp" and a storm id ("al092026") open on that storm instead of the saved basin and the first storm
    explicit HurricaneViewer(Window * parent, const string& basin = {}, const string& stormId = {});

private:
    void loadList();
    void loadStorm();
    void loadRecon();
    void loadEnsembles();
    void loadShips();
    void loadPod();
    void loadDrops();
    const UtilityDropsonde::Drop * dropAt(const QPointF& pixels) const;   // the dropsonde marker under the pointer
    bool dropNear(const UtilityDropsonde::Drop&) const;                    // in time and place for the storm on show
    void loadOutlook();
    void loadWsp();
    void loadGis();
    void loadVdm();
    void openSeason();
    void updateChanges();                      // compare this advisory with the last one seen
    string basinCode() const;                  // "al", "ep" or "cp"
    void showStorm();                       // after a storm has loaded: the panel text, the map region
    void zoomToStorm();
    void updateInfo();
    void paintMap(QPainter&);
    void paintPlannedRecon(QPainter&);
    void paintOutlook(QPainter&);
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
    ComboBox comboBasin;                                      // Atlantic, Eastern Pacific, Central Pacific
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
    QCheckBox * barbCheck{};
    string startStorm;   // from the dashboard: the storm to select when the first list arrives
    QCheckBox * wwCheck{};                                    // watches and warnings
    QCheckBox * coneCheck{};                                  // the forecast cone
    QCheckBox * swathCheck{};                                 // the forecast wind swath
    QCheckBox * radiiCheck{};
    QCheckBox * fixCheck{};                                   // the recon centre fixes (vortex messages)
    QCheckBox * autoCheck{};                                  // refresh every 10 minutes and alert on a new advisory
    QCheckBox * outlookCheck{};                               // the Tropical Weather Outlook areas
    QCheckBox * dropLabelCheck{};                             // the lowest pressure and strongest wind written beside each dropsonde
    QCheckBox * dropCheck{};                                  // dropsonde markers (click: the sounding)
    QCheckBox * podCheck{};                                   // the planned recon flights                                 // the wind radii now
    QCheckBox * ensembleChecks[3]{};                          // AIFS ENS members, IFS ENS members, the unperturbed runs
    Button buttonStats;
    Button buttonShips;
    Button buttonPod;
    Button buttonVdm;
    Button buttonIntensity;
    Button buttonSeason;
    Button buttonText;
    Button buttonOutlook;
    ComboBox comboRecon;                                      // what colours the flight tracks
    std::vector<vector<std::pair<float, float>>> coast;       // the basin's coastlines and borders (lon, lat)
    vector<HurricaneData::StormEntry> entries;
    std::shared_ptr<HurricaneData::StormData> storm;
    std::shared_ptr<HurricaneData::ReconData> recon;
    std::shared_ptr<HurricaneData::EnsembleData> ensembles;
    std::shared_ptr<HurricaneData::ShipsData> ships;
    std::shared_ptr<HurricaneData::PodData> pod;
    std::shared_ptr<HurricaneData::GisData> gis;
    std::shared_ptr<HurricaneData::DropData> drops;
    std::shared_ptr<HurricaneData::OutlookData> outlook;
    std::shared_ptr<HurricaneData::WspData> wsp;
    std::shared_ptr<HurricaneData::VdmData> vdm;
    QTimer refreshTimer;
    vector<string> changeLines;                               // what differs from the advisory before
    string changeTitle;
    const void * swathFor{nullptr};                           // the storm the cached swaths belong to
    std::array<QList<QPolygonF>, 3> swaths;             // 34, 50, 64 kt: the joined fields, (lon, lat) points
    string hoverTech;
    QString lastHoverText;                                    // what the hover shows now, kept while the pointer stays near its line
    string lastHoverTech;                                         // the guidance line under the pointer, drawn heavier
    int generation{0};
    bool closed{false};
    bool filling{false};                                      // the combo is being filled: its change is not a pick
};

#endif  // HURRICANEVIEWER_H
