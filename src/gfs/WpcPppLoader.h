// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef WPCPPPLOADER_H
#define WPCPPPLOADER_H

#include <string>
#include <vector>
#include "gfs/GfsData.h"

namespace WpcPppLoader {
    // The Weather Prediction Center's probabilistic precipitation portal (CONUS) as a Model Viewer model ("WPC-PPP", the loader of the chart registry's source of that id). Each product is a shapefile of
    // contour bands (Web Mercator, a zip): the amount of rain (qpf), snow (sn) or freezing rain (zr) in a period of 24, 36, 48 or 72 hours that is expected, reached at a percentile of the blend's
    // members, or the chance of an amount. The shapes are filled onto a 0.05 degree grid. The forecast "hour" is the end of the period, in hours from the run (the portal's own number, f022 ...).
    // The want's variable is "<hours>|<element>|<kind>" ("24|sn|prob_4p00", "48|qpf|p50", "24|zr|expected").
    void registerLoader();
    // The hours (the periods' ends) the portal has for a run and a product, from the directory's listing (kept for a few minutes); empty when it could not be read
    std::vector<int> hours(const GfsData::Run& run, const std::string& variable);
}

#endif  // WPCPPPLOADER_H
