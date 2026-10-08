// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSRENDER_H
#define GFSRENDER_H

#include <string>
#include <QByteArray>

// The app's side of the GFS charts: the network, the GDAL folder, the borders and the user's units, handed to GfsData / GfsChart.
namespace GfsRender {
    // true when the chart for this model screen product code ("500_wnd_ht") is drawn here rather than fetched as a picture
    bool handles(const std::string& model, const std::string& param);
    // The chart as PNG bytes; empty with the reason in error. cycle is the model screen's run ("12Z", or ""/"latest"); hour is the forecast hour.
    QByteArray png(const std::string& param, const std::string& sector, const std::string& cycle, int hour, std::string& error);
}

#endif  // GFSRENDER_H
