// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSRENDER_H
#define GFSRENDER_H

#include <string>
#include <QByteArray>
#include <QString>

// The app's side of the GFS charts: the network, the GDAL folder, the borders and the user's units, handed to GfsData / GfsChart.
namespace GfsRender {
    // The cache of one model screen: the decoded grids and the GRIB records fetched so far, in a temporary folder that is deleted when the session ends (the screen closes)
    class Session {
    public:
        Session();
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        QString folder() const;
        // the GRIB file assembled from what has been downloaded for the run and hour ("" if none): a valid GRIB2 file, a subset of the model's own
        QString partialGrib(const std::string& cycleRun, int hour) const;
    private:
        QString path;
    };

    // true when the chart for this model screen product code ("500_wnd_ht") is drawn here rather than fetched as a picture
    bool handles(const std::string& model, const std::string& param);
    // The chart as PNG bytes; empty with the reason in error. cycle is the model screen's run ("12Z", or ""/"latest"); hour is the forecast hour.
    // model is "GFS" or "NBM"
    QByteArray png(Session& session, const std::string& model, const std::string& param, const std::string& sector, const std::string& cycle, int hour, std::string& error);
    // the newest published run of the model as the screen writes a run ("12Z"); false when none is found (no connection)
    bool latestCycle(const std::string& model, std::string& cycle);
}

#endif  // GFSRENDER_H
