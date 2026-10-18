// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GOESHISTORYLAYER_H
#define GOESHISTORYLAYER_H

#include <map>
#include <set>
#include <QDateTime>
#include <QImage>
#include "mapkit/MapLayer.h"

// GOES imagery of any past time over the United States, from the Iowa Mesonet's archive of 4 km composites (GOES East and West joined, infrared, visible and water vapor, every 15
// minutes back to about 2010). The map's time bar steps through the hours around the time asked for. From 2017 the frames are the NOAA open data (AWS) GOES-East CONUS sector, 2 km, read through GDAL; the Iowa Mesonet files are the fallback.
class GoesHistoryLayer : public MapLayer {
public:
    GoesHistoryLayer();
    string id() const override { return "satellite/goes-history"; }
    string path() const override { return "Satellite/GOES history, United States (Iowa Mesonet archive, 4 km)"; }
    string source() const override { return "NOAA GOES-East on AWS (2017 on), Iowa Environmental Mesonet GOES composites"; }
    string tip() const override { return "GOES infrared, visible or water vapor imagery of a past time (or the last hours), 4 km, every 15 minutes: the time bar steps through them. A composite of GOES East and West, so a seam shows where they join"; }
    bool underCoast() const override { return true; }
    int order() const override { return 7; }
    void paint(QPainter&, MapHost&) override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void prepareTimes(MapHost&) override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;
    void setRange(const QDateTime& from, const QDateTime& to, MapHost&) override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { host.timesChanged(); host.redraw(); }

private:
    QDateTime centre() const;                       // the time the frames are around: asOf, or the newest quarter hour that is there (40 minutes ago)
    void request(MapHost&, long seconds);
    QImage decode(const QByteArray& bytes) const;
    string urlOf(long seconds) const;
    int product{0};          // 0 infrared, 1 visible, 2 water vapor
    int opacity{100};
    bool enhance{true};
    QDateTime asOf;
    long rangeFrom{0}, rangeTo{0};   // a loop from one time to another (seconds); 0: the hours around asOf
    long shown{0};           // the frame on view (seconds), 0: the newest of the window
    std::map<std::string, QImage> frames;   // product|seconds
    std::set<std::string> asked;
    string error;
};

#endif  // GOESHISTORYLAYER_H
