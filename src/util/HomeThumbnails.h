// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HOMETHUMBNAILS_H
#define HOMETHUMBNAILS_H

#include <string>
#include <vector>
#include <QByteArray>

using std::string;
using std::vector;

// The pictures the home screen can show as thumbnails: for each tool, its most recent product. Most are plain image
// downloads; the GRIB tools are rendered from the latest model run. Clicking a thumbnail opens its tool (routeId is
// the Toolbar entry's id). Pictures with transparent areas (SPC mesoanalysis, soundings, model renders, ...) are
// drawn on a white background so their dark lines and text stay readable on any theme.
namespace HomeThumbnails {
    struct Entry {
        string token;       // preference key and identity
        string label;       // in Settings
        string routeId;     // Toolbar::RouteItem::id opened on click ("" = special handling in MainWindow)
        bool white;         // composite onto white
        bool defaultOn;
    };

    const vector<Entry>& all();
    const Entry * find(const string& token);
    // blocking: the latest picture as image bytes (a download, or a render for the GRIB entries); empty on failure.
    // Use off the UI thread.
    QByteArray fetch(const string& token);
}

#endif  // HOMETHUMBNAILS_H
