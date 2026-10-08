// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SURFACEDATA_H
#define SURFACEDATA_H

#include <string>
#include <vector>
#include "obs/SurfaceStation.h"

using std::string;
using std::vector;

// The downloads behind the surface observations screen. These block, so call them off the GUI thread.
class SurfaceData {
public:
    // every current METAR (the Aviation Weather Center cache, with the site names); kept for two minutes
    static bool loadMetars(vector<SurfaceStation>& stations, string& error);
    // the newest report of each station of the MADIS mesonet files (the last two hourly files, about 30 MB packed each); kept for 15 minutes.
    // Stations at the same place as an airport report are left out.
    static bool loadMesonet(vector<SurfaceStation>& stations, string& error, const vector<SurfaceStation>& airports);
    static string mesonetUrl() { return "https://madis-data.ncep.noaa.gov/madisPublic1/data/LDAD/mesonet/netCDF/"; }
};

#endif  // SURFACEDATA_H
