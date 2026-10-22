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

// The cyclone phase space (Hart 2003) of a storm or invest in a deterministic model: the model's own heights followed with the storm, the state it is in now and the path it takes on the two
// diagrams (B against -VTL: symmetric or not; -VTL against -VTU: warm core or cold, and how deep). The model is picked from those that have pressure level heights.
class PhaseViewer : public Window {
public:
    // lat, lon: where the storm is now
    // atcfId ("al912026", "wp292026"): the observed track of the storm is fetched (NHC, else the RAL archive of the b-decks) and drawn on the map; the past is then looked at where it was, not where the model had it
    PhaseViewer(Window * parent, const string& title, double lat, double lon, const string& model = "GFS", const string& atcfId = {});
    // the models that can be measured: the deterministic ones with heights on pressure levels
    static vector<string> models();

private:
    void measure();
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * modelBox{};
    QLabel * status{};
    QLabel * headline{};
    PhasePlot * plot{};
    PhaseMap * map{};
    QTableWidget * table{};
    std::shared_ptr<GfsRender::Session> session;
    double lat, lon;
    string atcfId;
    std::shared_ptr<vector<PhaseSpace::Observed>> observed{std::make_shared<vector<PhaseSpace::Observed>>()};
    int generation{0};
    bool closed{false};
};

#endif  // PHASEVIEWER_H
