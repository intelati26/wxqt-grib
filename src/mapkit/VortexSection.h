// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef VORTEXSECTION_H
#define VORTEXSECTION_H

#include <functional>
#include <string>
#include <vector>
#include <QDateTime>
#include <QImage>

// The polar vortex through the season in two dimensions: the zonal-mean zonal wind on the circle of 60 degrees, one row for each pressure level, one column for each time (the initial state of
// the run at that time). A reversal of the stratospheric winds (a sudden warming) shows as the colors going from the top of the picture down. The levels are those the model's charts have winds
// for (a chart "<level>_wnd_ht"): a level with no wind is left out, and the rows are the levels, not an interpolation between them.
namespace VortexSection {
    struct Series {
        std::vector<QDateTime> when;
        std::vector<int> levels;                    // mb, the top of the atmosphere first
        std::vector<std::vector<double>> wind;      // [level][time], m/s, NaN where there is none
        bool south{false};
    };

    // The mean wind of the circle of 60 degrees (north, or south) of the model's initial state at each time, for each level; blocking (a read of the fields of every level at every time): call
    // it from a thread. `progress(i, n)` is asked before each time and says false to stop. False when stopped or when nothing was read.
    bool collect(const std::string& model, const std::vector<QDateTime>& when, const std::vector<int>& levels, bool south, Series& out, const std::function<bool(size_t, size_t)>& progress);

    // The planetary waves of one level: the amplitude of wave 1 and of wave 2 of the height on the circle of 60 degrees (a Fourier series of the heights every 2.5 degrees of longitude), and
    // the zonal-mean wind there, at each time. A vortex displaced toward a continent is wave 1; a split one is wave 2: the zonal mean wind can stay positive through both.
    struct Waves {
        std::vector<QDateTime> when;
        int level{0};                 // mb
        bool south{false};
        std::vector<double> wave1, wave2, wind;   // heights in metres, wind m/s; NaN where there is none
    };
    bool collectWaves(const std::string& model, const std::vector<QDateTime>& when, int level, bool south, Waves& out, const std::function<bool(size_t, size_t)>& progress);
    // The two amplitudes as lines, the wind as a thin gray one (its own scale at the right).
    QImage drawWaves(const Waves& waves, int width, const QString& title);

    // The picture: the rows at the left labelled with the level, the colors by the wind (negative, the reversed winds, in blues; positive in reds), the dates under, a color bar. `width` in pixels.
    QImage draw(const Series& series, int width, const QString& title);
}

#endif  // VORTEXSECTION_H
