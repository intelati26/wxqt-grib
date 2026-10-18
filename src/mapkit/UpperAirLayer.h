// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UPPERAIRLAYER_H
#define UPPERAIRLAYER_H

#include <QDateTime>
#include "mapkit/MapLayer.h"

// The balloon (radiosonde) stations as buttons on the map; a click opens the observed sounding of that station for the launch the time bar is at (00 and 12 UTC, steps of 12 hours). The soundings are
// the Iowa Environmental Mesonet's archive of the upper air network, which goes back decades (the SPC pages keep only the recent years).
class UpperAirLayer : public MapLayer {
public:
    string id() const override { return "obs/upper-air"; }
    string path() const override { return "Observations/Upper air soundings (balloon launches, past and present)"; }
    string source() const override { return "Iowa Environmental Mesonet RAOB archive"; }
    string tip() const override { return "The balloon launch sites: click one for the observed sounding (skew-T, hodograph, parameters) of the launch at the time on the bar, 00 and 12 UTC. The archive reaches back decades"; }
    int order() const override { return 76; }
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    string summary() const override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;

    // the archive's sounding of a station ("BMX") and launch (seconds): false with the reason
    static bool fetchProfile(const string& code, long launch, class SoundingProfile& out, string& why);

private:
    long launch() const;   // the launch on view (seconds)
    QDateTime asOf;
    long shown{0};
};

#endif  // UPPERAIRLAYER_H
