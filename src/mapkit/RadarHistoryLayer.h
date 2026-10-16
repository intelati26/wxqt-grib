// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RADARHISTORYLAYER_H
#define RADARHISTORYLAYER_H

#include <map>
#include <set>
#include <QDateTime>
#include <QImage>
#include "mapkit/MapLayer.h"

// The national radar mosaic of any past time, from the Iowa Mesonet's archive of the composites (the 5 minute base reflectivity picture, about 1 km back to 2010 and 2 km before that,
// to the late 1990s). It is the radar of the dates before the open MRMS bucket begins (14 October 2020); the time bar steps through the hour either side of the time asked for.
class RadarHistoryLayer : public MapLayer {
public:
    RadarHistoryLayer();
    string id() const override { return "radar/composite-history"; }
    string path() const override { return "Radar and precipitation/Radar mosaic of the past (Iowa Mesonet archive)"; }
    string source() const override { return "Iowa Environmental Mesonet NEXRAD composites (NWS radars)"; }
    string tip() const override { return "The national radar picture of a past time, every 5 minutes, back to the late 1990s (the MRMS layer reaches back only to October 2020). Base reflectivity; the time bar steps through the hour either side"; }
    bool underCoast() const override { return true; }
    int order() const override { return 11; }
    void paint(QPainter&, MapHost&) override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void prepareTimes(MapHost&) override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { host.timesChanged(); host.redraw(); }

private:
    struct Frame {
        QImage image;                    // the palette picture, thinned to about 1.6 km cells (the strongest echo of each block)
        double west{0.0}, north{0.0};    // the picture's upper left corner (degrees)
        double cellX{0.0}, cellY{0.0};   // degrees per pixel
        string kind;                     // "n0q" or "n0r"
    };
    QDateTime centre() const;
    void request(MapHost&, long seconds);
    static bool decode(const QByteArray& png, const QByteArray& world, Frame&);
    static string urlOf(const char * kind, long seconds, const char * extension);
    int opacity{100};
    QDateTime asOf;
    long shown{0};
    std::map<long, Frame> frames;
    std::set<long> asked;
    int missing{0};
    string error;
};

#endif  // RADARHISTORYLAYER_H
