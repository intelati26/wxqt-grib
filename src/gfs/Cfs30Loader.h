// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CFS30LOADER_H
#define CFS30LOADER_H

namespace Cfs30Loader {
    // The CFSv2's next 30 days against its own normal as a Model Viewer model ("CFS30", the loader of the chart registry's source of that id): the member 1 of the four newest runs sampled at the four
    // synoptic times of every third day from tomorrow (160 fields), their mean minus the mean of the CFS normals (NOAA's reforecast climatology) of the same days. A field of the CFS's grid in the shown unit
    // (degrees C, mm/day); the want's variable is the CFS key ("t2m", "precip"). The fields are those of NOAA's open bucket (byte ranges, kept), no web service with a quota.
    void registerLoader();
}

#endif  // CFS30LOADER_H
