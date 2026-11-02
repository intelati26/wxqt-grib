// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MPASNSSLLOADER_H
#define MPASNSSLLOADER_H

#include <string>
#include "gfs/GfsData.h"

namespace MpasNsslLoader {
    // NSSL's experimental MPAS run started from the HRRR ("MPAS-HTPO-NSSL", the model id "MPAS-HT"), as a Model Viewer model. NSSL posts each run as one tar of gzipped GRIB2 files, one
    // per forecast hour (data.nssl.noaa.gov, FRDD/NSSL-MPAS): the tar is read by range, so only the hour wanted is fetched (about 100 MB, 140 MB unpacked), kept in the cache folder, and the
    // field cut out of it and warped to a latitude / longitude grid by GDAL. Fields are asked for by the NOAA names (TMP, "2 m above ground") as for the HRRR.
    void registerLoader();
    // the first bytes of a run's tar say the run is there (a tar's first entry is "./<run>/")
    std::string tarUrl(const GfsData::Run& run, const std::string& tag = "mpasht2");   // "mpasht2" (started from the HRRR) or "mpasrn" (the other run, 00 and 12Z)
}

#endif  // MPASNSSLLOADER_H
