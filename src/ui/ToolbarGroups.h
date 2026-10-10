// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TOOLBARGROUPS_H
#define TOOLBARGROUPS_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// How the main-screen toolbar is shown, and the named groups its entries are sorted into. The groups head the
// sections of the icons + text toolbar and are the drop-down menus of the menu bar. Everything is the user's to
// change under Settings > Toolbar Order; the built-in grouping is only the starting point (and what a reset
// returns to). Entries are identified by RouteItem::id. Saved as two prefs.
namespace ToolbarGroups {
    enum Mode {
        Icons = 0,       // the original column of icons
        IconsText = 1,   // icons with their names, under group headings
        MenuBar = 2,     // a menu bar of group menus
    };

    struct Group {
        string name;
        vector<string> ids;
    };

    int mode();
    void setMode(int);
    vector<string> modeLabels();

    // `allIds`: every entry the toolbar has, in its current order. Builds the groups from the saved pref; entries
    // it does not mention (new since it was saved) go to their built-in group.
    void load(const vector<string>& allIds);
    const vector<Group>& groups();
    int groupOf(const string& id);
    void moveItem(const string& id, int group);
    void renameGroup(int group, const string& name);
    void addGroup(const string& name);
    void deleteGroup(int group);      // its entries move to the group before it (after it for the first)
    void moveGroup(int from, int to);
    void reset();                     // back to the built-in grouping (and the built-in dropdowns)

    // In the icon column ("Icons only"), a group can be one button that opens a menu of its entries instead of a button each: fewer icons on the home screen. The Dashboards
    // group is one by default. The other styles already show groups as headings or menus, so this is only used by the icon column.
    bool isDropdown(const string& groupName);
    void setDropdown(const string& groupName, bool on);
}

#endif  // TOOLBARGROUPS_H
