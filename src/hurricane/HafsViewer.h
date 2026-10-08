// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HAFSVIEWER_H
#define HAFSVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include "gfs/GfsRender.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

// NCEP's hurricane model (HAFS-A and HAFS-B) for one storm: its 2 km storm-following grid drawn as wind and pressure, simulated radar and satellite, rain, sea surface
// temperature, shear and vorticity, and waves. The model is run only for the active storms and invests, so the storm list is whatever has files in the newest cycle.
// `storm` is the NHC id ("al092026") of the one to show first, "" for the first in the list.
class HafsViewer : public Window {
public:
    HafsViewer(Window * parent, const string& storm = "", const string& name = "");
    // the model's own id of an NHC storm id: "al092026" is "09l", "ep182026" is "18e"
    static string modelId(const string& nhcId);

private:
    void loadStorms();
    void fillStorms(const std::vector<string>& found, const string& cycle);
    void draw();
    void step(int by);
    string model() const { return comboModel.getIndex() == 1 ? "HAFSB" : "HAFSA"; }
    std::vector<string> storms;
    std::vector<string> productIds;   // before the product list that fills it
    HBox row;
    VBox box;
    Photo photo;
    ComboBox comboModel;
    ComboBox comboStorm;
    ComboBox comboProduct;
    ComboBox comboTime;
    BackForward backForward;
    Text textStatus;
    string first;            // the storm to show when the list arrives
    string firstName;
    int drawing{0};
    int loading{0};
    std::shared_ptr<GfsRender::Session> session;
};

#endif  // HAFSVIEWER_H
