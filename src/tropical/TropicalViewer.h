// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TROPICALVIEWER_H
#define TROPICALVIEWER_H

#include <deque>
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

// The tropical master screen: every active tropical cyclone in the world (CIRA / RAMMB's list, with a current infrared picture of
// each), grouped by basin; click one for its page. The NHC tool (Atlantic, East and Central Pacific outlooks, advisories, SST) is a
// child of this screen, one button away.
class TropicalViewer : public Window {
public:
    explicit TropicalViewer(Window * parent);

private:
    void reload();
    void build(const vector<UtilityCira::Storm>&, const std::vector<string>& infraredUrls);
    void closeEventCustom() override { closed = true; }
    VBox box;
    ScrolledWindow sw;
    HBox rowTop;
    Button buttonNhc;
    Button buttonClimate;
    Button buttonRefresh;
    Text textNote;
    VBox boxStorms;
    std::deque<Text> headings;
    std::deque<FlowBox> flows;
    std::deque<Image> images;
    std::deque<Text> captions;
    std::deque<VBox> tileBoxes;
    bool closed{false};
};

#endif  // TROPICALVIEWER_H
