// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CLIMATEVIEWER_H
#define CLIMATEVIEWER_H

#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "climate/UtilityClimate.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/FlowBox.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The climate and ocean dashboard: the ENSO status line (El Niño / La Niña, from the Climate Prediction Center), charts of the
// index series behind the cycles (RONI, ONI, Niño regions, SOI, PDO, NAO, AO, PNA, AAO), then pictures in sections: sea surface
// temperature (raw and anomaly), marine heat, the ENSO discussion's figures and the tropical convection (MJO). Click a picture to
// enlarge it; the buttons open the source tables as text.
class ClimateViewer : public Window {
public:
    explicit ClimateViewer(Window * parent);

private:
    void reload();
    void build();
    void rebuildCharts();
    void openText(const UtilityClimate::TextProduct&);
    void closeEventCustom() override { closed = true; }
    struct Data {
        UtilityClimate::EnsoStatus enso;
        bool ensoOk{false};
        string ensoError;
        vector<UtilityClimate::Series> series;   // parallel to UtilityClimate::indices()
    };
    VBox box;
    ScrolledWindow sw;
    HBox rowTop;
    Button buttonRefresh;
    Button buttonDiscussion;
    Text textYears;
    ComboBox comboYears;
    FlowBox rowText;
    Text textStatus;
    Text textSynopsis;
    Text textNext;
    Text headingCharts;
    FlowBox flowCharts;
    VBox boxSections;
    std::deque<Button> textButtons;
    std::deque<Text> headings;
    std::deque<FlowBox> flows;
    std::deque<Image> images;
    std::deque<Text> captions;
    std::deque<VBox> tileBoxes;
    std::shared_ptr<Data> data;
    bool built{false};
    bool closed{false};
};

#endif  // CLIMATEVIEWER_H
