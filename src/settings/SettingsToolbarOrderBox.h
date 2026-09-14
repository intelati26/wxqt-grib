// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSTOOLBARORDERBOX_H
#define SETTINGSTOOLBARORDERBOX_H

#include <vector>
#include "ui/Button.h"
#include "ui/Text.h"
#include "ui/Toolbar.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::vector;

// Settings tab that lets the user reorder the main-screen toolbar tiles
// (up/down, wrapping at each end); reads/writes directly through the live
// Toolbar so the main screen updates immediately.
class SettingsToolbarOrderBox : public Widget {
public:
    SettingsToolbarOrderBox(Window *, Toolbar *);
    void refresh();

private:
    void addItems();
    void moveDownClicked(int);
    void moveUpClicked(int);
    VBox box;
    Window * parent;
    Toolbar * toolbar;
    vector<Button> buttons;
    vector<Text> labels;
    vector<HBox> hboxList;
};

#endif  // SETTINGSTOOLBARORDERBOX_H
