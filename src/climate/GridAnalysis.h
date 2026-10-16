// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GRIDANALYSIS_H
#define GRIDANALYSIS_H

#include <vector>

// Values at scattered points (stations) made into a field on a latitude / longitude grid: each cell is the average of the points within reach, weighted by how near they are (Cressman:
// (R^2 - d^2) / (R^2 + d^2)); a cell with too few points near it is left empty (NaN), so the field stops where the stations do. Pure arithmetic, no data source: any measure at any
// set of points can use it. Rows run from the north.
namespace GridAnalysis {
    struct Point {
        double lat;
        double lon;
        float value;
    };
    struct Spec {
        double west{0.0}, south{0.0}, east{0.0}, north{0.0};
        double step{0.1};            // degrees
        double reachKm{200.0};       // points farther than this from a cell do not count for it (the widest reach)
        double baseKm{0.0};          // above 0: the usual reach, shorter where stations are close (finer detail); it grows, up to reachKm, until `fewest` points near (and the 4th nearest) are in
        double nearestKm{90.0};      // a cell with no point this near is left empty
        int fewest{2};               // and one with fewer than this many within reach
    };
    struct Field {
        Spec spec;
        int columns{0};
        int rows{0};
        std::vector<float> values;   // columns x rows, row 0 the northern; NaN where there is no analysis
        bool empty() const { return values.empty(); }
        float at(double lat, double lon) const;   // NaN outside or where empty
    };
    Field analyze(const std::vector<Point>& points, const Spec& spec);
}

#endif  // GRIDANALYSIS_H
