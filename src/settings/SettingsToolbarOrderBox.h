// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSTOOLBARORDERBOX_H
#define SETTINGSTOOLBARORDERBOX_H

#include <vector>
#include "ui/Button.h"
#include "ui/Text.h"
#include <deque>
#include "ui/ComboBox.h"
#include "ui/Entry.h"
#include "ui/Toolbar.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::vector;

// Settings tab that lets the user arrange the main-screen toolbar: drag the entries to reorder them, drag the groups
// to reorder those, double-click a group to rename it, right-click an entry to put it in another group. Reads/writes
// directly through the live Toolbar so the main screen updates immediately.
class SettingsToolbarOrderBox : public Widget {
public:
    SettingsToolbarOrderBox(Window *, Toolbar *);
    void refresh();

private:
    void addItems();
    void addStyleAndGroups();
    void changed();   // after any edit: redraw the toolbar and this tab
    VBox box;
    Window * parent;
    Toolbar * toolbar;
    std::deque<Button> buttons;
    vector<Text> labels;
    vector<HBox> hboxList;
    // deques: elements must not move once their views are in the layout
    std::deque<ComboBox> combos;
    std::deque<Entry> entries;
};

#endif  // SETTINGSTOOLBARORDERBOX_H
