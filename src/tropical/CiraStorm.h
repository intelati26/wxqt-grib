// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CIRASTORM_H
#define CIRASTORM_H

#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "tropical/UtilityCira.h"
#include "ui/Button.h"
#include "ui/FlowBox.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// One storm's CIRA / RAMMB page: the newest satellite and guidance pictures (click one to enlarge), loops of the infrared and
// microwave imagery, the forecast track and the track history. Experimental products.
class CiraStorm : public Window {
public:
    CiraStorm(Window * parent, const string& stormId, const string& title);

private:
    void closeEventCustom() override { closed = true; }
    void fill(const UtilityCira::StormPage&);
    void addJtwc();                                  // the Joint Typhoon Warning Center's products, for the basins it covers
    void openText(const string& url, const string& heading);
    void addJma();   // the Japan Meteorological Agency's analysis and forecast in English (western Pacific storms)
    string stormId;
    string title;
    VBox box;
    ScrolledWindow sw;
    HBox rowButtons;
    Text textHeader;
    Text textNote;
    FlowBox flowImages;
    Text textForecast;
    Text textHistory;
    Text textRapid;
    std::deque<Button> buttons;
    std::deque<Image> images;
    bool closed{false};
};

#endif  // CIRASTORM_H
