// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EC46LOADER_H
#define EC46LOADER_H

namespace Ec46Loader {
    // The ECMWF's extended range (EC46) and seasonal (SEAS5) forecasts as Model Viewer models: the loaders of the chart registry's "EC46" and "SEAS5" sources (GfsData::setLoader). A forecast "hour" is a week of the forecast
    // (168 hours each: 0 the week of the run, 168 the next ...; for the SEAS5 a month, 720 hours each); the field is the ensemble mean of that week at the points of a 4 degree grid over North America, from Open-Meteo's seasonal
    // API, spread bicubic onto a half degree grid.
    void registerLoader();
}

#endif  // EC46LOADER_H
