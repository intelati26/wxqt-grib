// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYJTWC_H
#define UTILITYJTWC_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The Joint Typhoon Warning Center's warning text (western Pacific, Indian Ocean and Southern Hemisphere): the warning position and each forecast position with the
// maximum wind and the 34 / 50 / 64 kt wind radii by quadrant - what JTWC's own graphic draws. Pure parsing here; the fetch is a plain text download.
namespace UtilityJtwc {
    struct Fix {
        int hour{0};                 // 0: the warning position
        double lat{0}, lon{0};       // degrees, east positive
        int knots{-1}, gusts{-1};    // 1 minute wind
        string validTime;            // "191800Z"
        double radiusNm[3][4]{};     // 34, 50, 64 kt by quadrant NE, SE, SW, NW; 0: none
    };
    // CIRA's "wp292026" is JTWC's "wp2926"; empty for an invest (number 90 and up) or a basin JTWC does not warn for
    string codeOf(const string& ciraId);
    string warningUrl(const string& code);
    vector<Fix> parseWarning(const string& text);
}

#endif  // UTILITYJTWC_H
