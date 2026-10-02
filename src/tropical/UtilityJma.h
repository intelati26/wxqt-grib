// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYJMA_H
#define UTILITYJMA_H

#include <string>

using std::string;

// The Japan Meteorological Agency is the official (WMO RSMC Tokyo) tropical cyclone centre for the western North Pacific. Its public
// data feed (the one behind JMA's own typhoon map, https://www.jma.go.jp/bosai/typhoon/data/) carries English names and categories but
// Japanese place names, movement and intensity words; the numbers are language-neutral. This builds an English analysis and forecast
// from it: the Japanese words are translated from a small table, and anything not in the table is shown as JMA wrote it.
namespace UtilityJma {
    // the storm with this name (case and hyphens ignored), "" when JMA has no such active storm; `text` is the English advisory
    bool advisoryFor(const string& stormName, string& text, string& error);
    // "Major Hurricane RACHEL" / "CHOI-WAN" -> the trailing upper-case name ("" for an invest)
    string nameFromTitle(const string& title);
}

#endif  // UTILITYJMA_H
