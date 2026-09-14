// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYTHEME_H
#define UTILITYTHEME_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// Application theme (light / dark / follow the OS).
// Persisted under the "THEME" preference as "system", "light" or "dark".
class UtilityTheme {
public:
    static const string pref;
    static const vector<string> labels;   // user facing, index matches values
    static const vector<string> values;   // pref tokens

    static void apply();                   // read pref and apply
    static void applyTheme(const string&); // apply a specific value
    static int prefIndex();                // index into labels/values for current pref
    static bool isDark();                  // effective dark state right now
};

#endif  // UTILITYTHEME_H
