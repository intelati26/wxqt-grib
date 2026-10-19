// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYJMA_H
#define UTILITYJMA_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The Japan Meteorological Agency is the official (WMO RSMC Tokyo) tropical cyclone centre for the western North Pacific. Its public
// data feed (the one behind JMA's own typhoon map, https://www.jma.go.jp/bosai/typhoon/data/) carries English names and categories but
// Japanese place names, movement and intensity words; the numbers are language-neutral. This builds an English analysis and forecast
// from it: the Japanese words are translated from a small table, and anything not in the table is shown as JMA wrote it.
namespace UtilityJma {
    // the storm with this name (case and hyphens ignored), "" when JMA has no such active storm; `text` is the English advisory
    bool advisoryFor(const string& stormName, string& text, string& error);
    // JMA's forecast positions of the storm with this name, each with the radius of its 70% probability circle (the storm's centre stays inside it 70% of the time): the
    // cone of uncertainty of a western Pacific storm (JTWC publishes none). Hour 0 is the analysis, radius 0.
    struct Circle {
        int hour{0};
        double lat{0}, lon{0};     // degrees, east positive
        double radiusKm{0};
        int knots{-1};             // JMA's 10 minute wind at the centre
        int gustKnots{-1};
        int pressure{0};           // hPa
        string category;           // JMA's class at that time: TD, TS, STS, TY, L (a low) ...
        string motion;             // "WNW at 11 kt (20 km/h)"
        string validUtc;           // "2026-10-20T06:00:00Z"
    };
    bool forecastCircles(const string& stormName, vector<Circle>& out);
    // "Major Hurricane RACHEL" / "CHOI-WAN" -> the trailing upper-case name ("" for an invest)
    string nameFromTitle(const string& title);
}

#endif  // UTILITYJMA_H
