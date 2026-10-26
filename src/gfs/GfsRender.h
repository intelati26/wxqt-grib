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
#include "sounding/SoundingProfile.h"
#include "tropical/PhaseSpace.h"
#include <functional>

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
    // The column of a model over a point for one run and forecast hour ("yyyyMMddHH"), as a sounding: the pressure levels 1000 to 100 mb (those the model has, above the ground) and the surface
    // (pressure, 2 m temperature and dewpoint, 10 m wind). Fields are read like a chart's (byte ranges, a box round the point where the server can cut one). False with the reason when too little came.
    bool columnProfile(Session& session, const std::string& model, const std::string& run, int hour, double lat, double lon, SoundingProfile& out, std::string& error);
    // The cyclone phase space (Hart 2003: B, -VTL, -VTU) of a storm in a deterministic model's run, hour by hour: the heights of 900 to 300 mb and the sea level pressure are read in a box round the
    // storm, whose centre is followed as the lowest pressure near where it was (`lat`, `lon` is where it is at `validTime`, "yyyyMMddHH" UTC; the run's hours before that are skipped, and the track
    // goes `hoursAhead` on in steps of six). `progress` is called with each point as it is done. False with the reason when no hour could be measured.
    bool phaseTrack(Session& session, const std::string& model, const std::string& run, const std::string& validTime, double lat, double lon, int hoursAhead,
                    std::vector<PhaseSpace::Point>& out, std::string& error, const std::function<void(const PhaseSpace::Point&)>& progress = {}, const std::string& storm = "");
    // true when the chart for this model screen product code ("500_wnd_ht") is drawn here rather than fetched as a picture
    bool handles(const std::string& model, const std::string& param);
    // A chart made from the chart's fill instead of the plain chart: the largest value of it over a list of forecast hours (a 24 hour maximum), or its change since the run
    // hoursBack hours earlier at the same valid time
    struct Variant {
        enum class Kind { None, Max, Change };
        Kind kind{Kind::None};
        std::vector<int> hours;
        int hoursBack{0};
        bool standardDeviations{false};   // an anomaly chart in standard deviations (GfsChart::withStandardDeviations)
        // the valid time to draw ("yyyyMMddHH", UTC): the forecast hour is then worked out from the run actually used, so models of other runs are compared at the same real time
        std::string validTime;
        std::string poolOnly;   // a pooled chart (ENS-ALL) from one of its ensembles' members only: "IFS-ENS", "GEFS" or "GEPS" (GfsChart::withPoolOnly)
        std::string member;   // one ensemble member instead of the mean (GfsChart::withMember); "" the chart as it is
        // other models whose lines are drawn over this one's, each in a color of its own, for the same valid time: the chart's first set of lines (heights, pressure) of each, and no fill
        std::vector<std::string> overlayModels;
        // the chart as a layer for the scrollable map: only the map part of the picture (no header, legend or frame, no coastlines or places), the ground left clear, `width` pixels wide (0: the default);
        // the probe then covers the picture whole
        // the fill's grid and its color scale instead of a picture (the globe draws them itself); the picture is then one pixel
        GfsGrid::Grid * fillOut{nullptr};
        GfsChart::Ramp * rampOut{nullptr};
        GfsGrid::Grid * windUOut{nullptr}, * windVOut{nullptr};   // with fillOut: the wind of the chart (m/s)
        std::vector<GfsChart::ContourOut> * contoursOut{nullptr};   // with fillOut: the chart's line sets
        bool noBarbs{false};      // no barbs in the picture (the layer draws them)
        GfsChart::VectorLines * vectorOut{nullptr};   // no contour lines, numbers or highs / lows in the picture: they are handed over here (the layer draws them)
        bool bare{false};
        int width{0};
    };
    // The chart as PNG bytes; empty with the reason in error. cycle is the model screen's run ("12Z", or ""/"latest"); hour is the forecast hour.
    // model is "GFS" or "NBM"
    QByteArray png(Session& session, const std::string& model, const std::string& param, const std::string& sector, const std::string& cycle, int hour, const std::vector<std::string>& overlays, std::string& error, GfsChart::Probe * probe = nullptr, const Variant& variant = {});
    // A look at a model's source for the settings page: the newest run the server has, and whether that run lists the fields of the model's first chart (no field is downloaded).
    struct SourceCheck {
        std::string model;
        std::string label;
        std::string run;       // "2026101018", empty when none was found
        bool ok{false};
        bool skipped{false};   // a model that needs more than its server (the hurricane model needs a storm)
        std::string detail;
        double seconds{0.0};
    };
    SourceCheck checkSource(Session& session, const std::string& model);
    // True when the chart cannot be made at the model's first forecast hour (hour 0): the run has no file for that hour (the REFS ensemble products start at hour 1), or the chart
    // is only an accumulation, which hour 0 has none of. The run's index is read, no field is downloaded; a lost connection or a run that is not there is not an answer (false), and
    // the file is only called missing when the next hour's is there. runId is "yyyyMMddHH".
    // the hours (of `hours`) for which the run's index lists every record the chart needs: probed in parallel, an index read once for all the charts; empty when nothing could be told (no connection, no run)
    std::vector<int> availableHours(Session& session, const std::string& model, const std::string& param, const std::string& runId, const std::vector<int>& hours, const std::string& storm = "");
    bool firstHourMissing(Session& session, const std::string& model, const std::string& param, const std::string& runId);
    // the newest published run of the model as the screen writes a run ("12Z"); false when none is found (no connection)
    // storm is the HAFS storm ("09l"); it is the "sector" of the HAFS charts, whose grid follows the storm
    bool latestCycle(const std::string& model, std::string& cycle, const std::string& storm = "", std::string * date = nullptr);
    // The forecast track of a storm from the hurricane model ("HAFSA" / "HAFSB") in its newest complete run, with the run's name ("12Z"); empty when the model has none for it
    std::vector<GfsChart::TrackPoint> hafsTrack(const std::string& model, const std::string& storm, std::string& cycle);
    // The storms the hurricane model ("HAFSA" or "HAFSB") has files for in its newest cycle, as NHC ids ("09l", "15e"), and that cycle ("12Z"); empty when none
    std::vector<std::string> hafsStorms(const std::string& model, std::string& cycle);
}

#endif  // GFSRENDER_H
