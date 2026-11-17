// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTRAIN_H
#define DROUGHTRAIN_H

#include <atomic>
#include <string>
#include <vector>
#include <QDateTime>

// The rain that fell over a number of days, from NOAA's MRMS multi-sensor (gauge-corrected radar) totals in the open bucket (noaa-mrms-pds, from October 2020): the 72 hour and 24 hour
// Pass2 files that end at 12 UTC added up, each kept (0.05 degree grid, 20 to 55 N, 130 to 60 W: the contiguous United States and the populated strip of Canada) so a day is fetched once.
// Where the radar and the gauges do not reach the cell has no value. (The part of the Canadian strip beyond the US gauges is mostly radar.)
namespace DroughtRain {
    struct Grid {
        double west{-130.0}, north{55.0}, step{0.05};
        int columns{1400}, rows{700};
        std::vector<float> mm;   // columns x rows, row 0 the northern; NaN where there is no coverage
        std::string error;
        int files{0};            // the files added up
        bool empty() const { return mm.empty(); }
    };
    // The newest 12 UTC the totals can end at (a file takes a couple of hours to be made).
    QDateTime latestEnd(const QDateTime& now = QDateTime::currentDateTimeUtc());
    // The rain (millimeters) of the `days` days that end at `end` (12 UTC). Blocking: call off the interface thread.
    // `done`, when given, counts the files read so far (for a progress line); pieces(days) is how many there will be.
    Grid total(const QDateTime& end, int days, std::atomic<int> * done = nullptr);
    int pieces(int days);
}

#endif  // DROUGHTRAIN_H
