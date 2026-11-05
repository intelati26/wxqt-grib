// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSLOADER_H
#define MRMSLOADER_H

namespace MrmsLoader {
    // MRMS's composite reflectivity as a Model Viewer model (the chart registry's "MRMS" source, GfsData::setLoader), so the models can be compared with what was observed: a run is an hour, its one
    // "forecast hour" 0 is the first scan of that hour (NOAA's open bucket, which keeps them from 2020), the field the 0.01 degree grid taken to 0.04 degrees by its largest value.
    void registerLoader();
}

#endif  // MRMSLOADER_H
