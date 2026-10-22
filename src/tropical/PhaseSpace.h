// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PHASESPACE_H
#define PHASESPACE_H

#include <map>
#include <string>
#include <vector>
#include "gfs/GfsGrid.h"

// Hart's (2003) cyclone phase space of a storm in a model's fields: B, the thickness asymmetry of 900-600 mb across the storm's track (m), and -VTL / -VTU, the lapse of the height
// range (highest minus lowest height within 500 km) with ln p through 900-600 and 600-300 mb (a warm core is above zero). Worked from the model's own heights, with the storm followed
// as the lowest sea level pressure near where it was.
namespace PhaseSpace {
    struct Point {
        int hour{0};
        long valid{0};                // seconds since the epoch, UTC
        bool bKnown{false};           // B needs the storm's motion: a lone analysis has none
        double lat{0.0}, lon{0.0};
        double pressure{0.0};         // hPa at the centre, 0 when not known
        double heading{-1.0};         // degrees clockwise from north the storm moves toward, -1 when not known
        double b{0.0}, vtl{0.0}, vtu{0.0};
        bool ok{false};               // false: too few levels, or the circle runs off the grid
        std::string state;            // see stateOf
    };
    // a fix of the observed (best) track
    struct Observed {
        long valid{0};            // seconds since the epoch, UTC
        double lat{0.0}, lon{0.0};
        int wind{-1};             // knots, -1 when not given
        int pressure{-1};         // hPa
    };
    // the fixes of an ATCF best track file (b-deck) in time order, one for each time
    std::vector<Observed> parseBest(const std::string& text);
    // where the observed track was at `valid` (seconds): false when that is outside it by more than `slack` seconds (the nearest end is given within it)
    bool observedAt(const std::vector<Observed>& track, long valid, long slack, double& lat, double& lon);
    // the heights by pressure level (hPa) as grids that cover the circle
    using Levels = std::map<int, GfsGrid::Grid>;
    // B, -VTL and -VTU of the storm at lat, lon moving toward `heading` (negative: not known, B then 0)
    Point measure(const Levels& heights, double lat, double lon, double heading);
    // the points with B, -VTL and -VTU (and the pressure) averaged over the points within `seconds` either side of each by valid time (a running mean; the smoother of the Tropical Tidbits diagrams)
    std::vector<Point> smoothed(const std::vector<Point>& points, long seconds);
    // "Symmetric warm core" (tropical), "Symmetric cold core", "Asymmetric warm core" (hybrid / warm seclusion), "Asymmetric cold core" (extratropical); thresholds B 10 m, -VTL 0 and -VTU 0
    std::string stateOf(const Point&);
    // the short word for a card: "Tropical", "Subtropical / hybrid", "Extratropical", "Warm seclusion", "Weak"
    std::string wordFor(const Point&);
    // the lowest pressure near `near` (within `radius` degrees; a smoothed field in Pa or hPa): false when the grid has none there
    bool lowestNear(const GfsGrid::Grid& pressure, double lat, double lon, double radius, double& foundLat, double& foundLon, double& value);
    // bearing in degrees, from one fix to another
    double bearing(double lat1, double lon1, double lat2, double lon2);
    double kilometres(double lat1, double lon1, double lat2, double lon2);
}

#endif  // PHASESPACE_H
