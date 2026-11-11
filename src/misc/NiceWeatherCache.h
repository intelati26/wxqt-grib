// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NICEWEATHERCACHE_H
#define NICEWEATHERCACHE_H

#include <memory>
#include <string>
#include <QByteArray>
#include <QDateTime>
#include "gfs/GfsChart.h"

// The Nice weather outlook's days, made once: the scoring of a day (nine ingredients smoothed over the whole country) is slow, and the summary of days 1 to 8, each day's page and the picture on the
// home screen all show the same day. A day is made by whichever asks first; the others take it. It is kept for NiceWeatherCache::freshMinutes (the blend makes a run every hour), or until forget().
class NiceWeatherCache {
public:
    static constexpr int freshMinutes = 30;
    struct Day {
        QByteArray png;                          // the chart, 1100 pixels wide (a smaller picture is this one scaled)
        std::string summary;                     // the text beside the map on the day's page
        std::string headline;                    // "Nice or better 31%, Beautiful or better 9%"
        QDateTime valid;                         // the day (the daytime, by the sun's time of each place)
        QDateTime made;
        std::shared_ptr<GfsChart::Probe> probe;  // the values under the pointer
    };
    // the day (0 is day 1 ... 7 is day 8) if it has been made lately, else null; nothing is drawn
    static std::shared_ptr<const Day> find(int day);
    // the day: the one made lately, else made now (blocking: off the screen's thread); null with `error` when it cannot be
    static std::shared_ptr<const Day> make(int day, std::string& error);
    static void forget();                        // every day (a refresh)
};

#endif  // NICEWEATHERCACHE_H
