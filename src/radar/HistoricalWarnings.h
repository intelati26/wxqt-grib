// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HISTORICALWARNINGS_H
#define HISTORICALWARNINGS_H

#include <vector>
#include <QDateTime>
#include "objects/LatLon.h"
#include "radar/PolygonType.h"

using std::vector;

// The storm-based warnings that were in effect at a past time, for the radar's history view: Iowa State's Iowa Environmental
// Mesonet archive (mesonet.agron.iastate.edu/geojson/sbw.py?ts=<UTC time>), which holds every warning polygon with its begin and
// end time (the same archive College of DuPage's pages draw from). Tornado, severe thunderstorm, flash flood, special marine,
// snow squall and dust storm warnings; a special weather statement has no polygon in the archive.
namespace HistoricalWarnings {
    // the outlines (in the app's lat / lon convention, west positive) of the warnings of `type` valid at `utc`; one download of
    // the archive serves every type for the same minute. Call it from a worker thread.
    vector<vector<LatLon>> polygonsAt(PolygonType type, const QDateTime& utc);
}

#endif  // HISTORICALWARNINGS_H
