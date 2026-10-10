// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PLACEHOLDERDASHBOARD_H
#define PLACEHOLDERDASHBOARD_H

#include <deque>
#include <functional>
#include <string>
#include <vector>
#include "ui/Button.h"
#include "ui/FlowBox.h"
#include "ui/HBox.h"
#include "ui/ScrolledWindow.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// A dashboard that is planned but not built: the panels it will have (what each shows, where its data comes from, which step it
// belongs to) laid out as dashed tiles, plus buttons that open what already exists for the subject. The screens that replace it
// keep the same toolbar entry.
class PlaceholderDashboard : public Window {
public:
    struct Panel {
        string title;
        string what;     // what the panel will show
        string source;   // where the data comes from
        string step;     // "Phase 1" ...
    };
    struct Link {
        string label;
        std::function<void(Window *)> open;   // gets this dashboard as the parent of whatever it opens
    };
    PlaceholderDashboard(Window * parent, const string& title, const string& intro, const vector<Panel>&, const vector<Link>& links = {});

private:
    VBox box;
    ScrolledWindow sw;
    Text textIntro;
    Text textNote;
    HBox rowLinks;
    FlowBox flow;
    std::deque<Button> linkButtons;
    std::deque<Text> texts;
    std::deque<VBox> tileBoxes;
};

#endif  // PLACEHOLDERDASHBOARD_H
