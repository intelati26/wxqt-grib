// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSHOMESCREENORDERBOX_H
#define SETTINGSHOMESCREENORDERBOX_H

#include <deque>
#include <string>
#include "settings/UIPreferences.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::deque;
using std::string;

// Settings tab that lets the user rearrange the home screen: the order of
// the image / forecast / text columns, and of the items within the image and
// text columns: drag a row to move it, tick a row to show it. Hidden items
// are listed too, so they keep their place when ticked again. The main
// screen picks the new order up when Settings closes.
class SettingsHomeScreenOrderBox : public Widget {
public:
    explicit SettingsHomeScreenOrderBox(Window *);
    void refresh();

private:
    void addItems();
    void addDragList(const string&, const string&, HomeScreenOrder&);
    static bool isShown(const string&);
    VBox box;
    Window * parent;
    // deques: elements must not move once their views are in the layout
    deque<Text> labels;
    deque<HBox> hboxList;
    deque<ComboBox> combos;
};

#endif  // SETTINGSHOMESCREENORDERBOX_H
