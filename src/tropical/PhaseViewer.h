// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PHASEVIEWER_H
#define PHASEVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <QTableWidget>
#include "gfs/GfsRender.h"
#include "tropical/PhaseSpace.h"
#include "ui/Window.h"
#include "ui/VBox.h"

using std::string;
using std::vector;

class PhasePlot;
class PhaseMap;

// The cyclone phase space (Hart 2003) of a storm, an invest or any swirl in a deterministic model: the model's own heights followed with the system, the state it is in now and the path it takes on the
// two diagrams (B against -VTL: symmetric or not; -VTL against -VTU: warm core or cold, and how deep). The model is picked from those that have pressure level heights.
//
// A window holds one or more systems to compare: Add... takes a place ("52.0 -12.0 Norwegian Sea low") or a storm's id ("al932026"), a pick on the master map joins the window that is open (see pick),
// and the systems chosen before are offered again (the last ten). The one picked in the list is shown in full, the others as thinner lines in their own colours on the same diagrams and map.
class PhaseViewer : public Window {
public:
    // lat, lon: where the system is now (0, 0 with an atcfId: the newest fix of its best track file)
    PhaseViewer(Window * parent, const string& title, double lat, double lon, const string& model = "GFS", const string& atcfId = {});
    // the way every selection of a system comes in: the window already open takes it as one more system to compare (and comes forward); without one a new window opens on it
    static PhaseViewer * pick(Window * parent, const string& title, double lat, double lon, const string& model = "GFS", const string& atcfId = {});
    // one more system in this window
    void addSystem(const string& title, double lat, double lon, const string& atcfId = {});
    // the models that can be measured: the deterministic ones with heights on pressure levels
    static vector<string> models();

private:
    struct System {
        string label;
        double lat{0.0}, lon{0.0};
        string atcfId;
        std::shared_ptr<vector<PhaseSpace::Observed>> observed{std::make_shared<vector<PhaseSpace::Observed>>()};
        vector<PhaseSpace::Point> points;
        string problem;
        bool measured{false};
        QColor colour;
    };
    void start(const std::shared_ptr<System>& system);   // its observed track (when it has an id), then its measurement
    void measure(const std::shared_ptr<System>& system);
    void measureAll();
    void refresh();                                      // the diagrams, the map, the headline and the table from what has been measured
    void askForSystem();
    std::shared_ptr<System> shown() const;
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * modelBox{};
    QComboBox * systemBox{};
    QLabel * status{};
    QLabel * headline{};
    PhasePlot * plot{};
    PhaseMap * map{};
    QTableWidget * table{};
    std::shared_ptr<GfsRender::Session> session;
    vector<std::shared_ptr<System>> systems;
    string runUsed;
    int generation{0};
    bool closed{false};
};

#endif  // PHASEVIEWER_H
