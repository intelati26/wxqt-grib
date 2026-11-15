// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The built-in server table: the text of servers/servers.json of the repository (a test compares the two). Edit the file and run python3 servers/make_builtin.py.
#include "gfs/Servers.h"

const std::string& Servers::builtInJson() {
    static const std::string text = R"JSON({
  "version": 1,
  "updated": "2026-11-15",
  "servers": {
    "rrfs-ops": {
      "bases": [
        "https://noaa-rrfs-ops-pds.s3.amazonaws.com",
        "https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com"
      ],
      "note": "NOAA RRFS, REFS and the RRFS ensemble: the operational open-data bucket"
    },
    "gfs-aws": {
      "bases": [
        "https://noaa-gfs-bdp-pds.s3.amazonaws.com",
        "https://noaa-gfs-bdp-pds.s3.us-east-1.amazonaws.com"
      ],
      "note": "NOAA GFS: the open-data bucket"
    },
    "ecmwf-gcs": {
      "bases": [
        "https://storage.googleapis.com/ecmwf-open-data"
      ],
      "note": "ECMWF open data (IFS, IFS-ENS, AIFS) replicated on Google's storage"
    },
    "geos-nccs": {
      "bases": [
        "https://portal.nccs.nasa.gov/datashare/gmao/geos-fp/forecast"
      ],
      "note": "NASA GMAO GEOS-FP forecast files"
    }
  }
}
)JSON";
    return text;
}
