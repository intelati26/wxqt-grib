// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SPCREFSLOADER_H
#define SPCREFSLOADER_H

// Registers the reader of SPC's REFS Zarr store as the loader of the chart registry's "SPCREFS" source (GfsData::setLoader): an array's slice for a forecast hour (and member), resampled from
// SPC's Lambert grid to the latitude / longitude grid the charts use. Called once at startup.
namespace SpcRefsLoader {
    void registerLoader();
}

#endif  // SPCREFSLOADER_H
