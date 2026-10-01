// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RADARFAVORITES_H
#define RADARFAVORITES_H

#include <functional>
#include <string>
#include <vector>

using std::string;
using std::vector;

// The user's favourite radar sites (codes such as "KTLX"), kept in the preference RADAR_FAVORITES.
namespace RadarFavorites {
    vector<string> list();
    bool contains(const string& site);
    void add(const string& site);
    void remove(const string& site);
    void toggle(const string& site);
    string label(const string& site);   // "KTLX: OK, Oklahoma City"
    // called after the list changes (the radar screen refreshes its favourites box); set by the open radar screen
    extern std::function<void()> onChanged;
}

#endif  // RADARFAVORITES_H
