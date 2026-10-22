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

// The cyclone phase space (Hart 2003) of a storm or invest in a deterministic model: the model's own heights followed with the storm, the state it is in now and the path it takes on the two
// diagrams (B against -VTL: symmetric or not; -VTL against -VTU: warm core or cold, and how deep). The model is picked from those that have pressure level heights.
class PhaseViewer : public Window {
public:
    // lat, lon: where the storm is now
    PhaseViewer(Window * parent, const string& title, double lat, double lon, const string& model = "GFS");
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
    QTableWidget * table{};
    std::shared_ptr<GfsRender::Session> session;
    double lat, lon;
    int generation{0};
    bool closed{false};
};

#endif  // PHASEVIEWER_H
