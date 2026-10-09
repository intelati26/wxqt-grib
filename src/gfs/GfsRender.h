// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSRENDER_H
#define GFSRENDER_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include "gfs/GfsChart.h"

// The app's side of the GFS charts: the network, the GDAL folder, the borders and the user's units, handed to GfsData / GfsChart.
namespace GfsRender {
    // The handle a model screen holds on the model data cache: the decoded grids and the GRIB records fetched, in a folder shared by every screen and kept between runs of the program (GfsCache:
    // older than 48 hours by default is removed when the program starts)
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

    // the bytes the model data cache holds, and remove them all (for the settings)
    // Tidies the shared field cache in the background, a few seconds after the program starts and then every few hours it stays open: what was downloaded more than the number of hours kept
    // ago (48 unless the settings say otherwise) goes, and the oldest go first if the folder is over its size limit. Nothing waits for it; call it once, from the main thread.
    void startCacheCleanup();
    qint64 cacheUsage();
    void clearCache();
    // true when the model's charts are drawn here at all (the model screen then offers the grouped picker instead of a plain list)
    bool drawsModel(const std::string& model);
    // true when the chart for this model screen product code ("500_wnd_ht") is drawn here rather than fetched as a picture
    bool handles(const std::string& model, const std::string& param);
    // A chart made from the chart's fill instead of the plain chart: the largest value of it over a list of forecast hours (a 24 hour maximum), or its change since the run
    // hoursBack hours earlier at the same valid time
    struct Variant {
        enum class Kind { None, Max, Change };
        Kind kind{Kind::None};
        std::vector<int> hours;
        int hoursBack{0};
    };
    // The chart as PNG bytes; empty with the reason in error. cycle is the model screen's run ("12Z", or ""/"latest"); hour is the forecast hour.
    // model is "GFS" or "NBM"
    QByteArray png(Session& session, const std::string& model, const std::string& param, const std::string& sector, const std::string& cycle, int hour, const std::vector<std::string>& overlays, std::string& error, GfsChart::Probe * probe = nullptr, const Variant& variant = {});
    // the newest published run of the model as the screen writes a run ("12Z"); false when none is found (no connection)
    // storm is the HAFS storm ("09l"); it is the "sector" of the HAFS charts, whose grid follows the storm
    bool latestCycle(const std::string& model, std::string& cycle, const std::string& storm = "", std::string * date = nullptr);
    // The forecast track of a storm from the hurricane model ("HAFSA" / "HAFSB") in its newest complete run, with the run's name ("12Z"); empty when the model has none for it
    std::vector<GfsChart::TrackPoint> hafsTrack(const std::string& model, const std::string& storm, std::string& cycle);
    // The storms the hurricane model ("HAFSA" or "HAFSB") has files for in its newest cycle, as NHC ids ("09l", "15e"), and that cycle ("12Z"); empty when none
    std::vector<std::string> hafsStorms(const std::string& model, std::string& cycle);
}

#endif  // GFSRENDER_H
