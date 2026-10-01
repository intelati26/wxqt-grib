// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingTornadoProb.h"
#include <vector>

namespace SoundingTornadoProb {
    namespace {
        struct Row {
            double below;   // the row applies while the value is below this
            double probability;
            int colour;
        };

        Result lookup(double value, const std::vector<Row>& rows, double last, int lastColour) {
            if (value <= -9998.0) return {};
            for (const auto& row : rows) {
                if (value < row.below) return {row.probability, row.colour};
            }
            return {last, lastColour};
        }
    }

    Result fromMlCape(double cape) {
        if (cape <= -9998.0) return {};
        if (cape == 0.0) return {0.0, 0};
        return lookup(cape, {{250, 0.12, 1}, {500, 0.14, 2}, {1000, 0.16, 2}, {1500, 0.15, 2}, {2000, 0.13, 2}, {2500, 0.14, 2},
                             {3000, 0.18, 3}, {4000, 0.20, 3}}, 0.16, 3);
    }

    Result fromMlLcl(double lcl) {
        return lookup(lcl, {{750, 0.19, 3}, {1000, 0.19, 3}, {1250, 0.15, 2}, {1500, 0.10, 1}, {1750, 0.06, 0}, {2000, 0.06, 0},
                            {2500, 0.02, 0}}, 0.0, 0);
    }

    Result fromEffectiveSrh(double srh) {
        return lookup(srh, {{50, 0.06, 0}, {100, 0.06, 0}, {200, 0.08, 1}, {300, 0.14, 2}, {400, 0.20, 3}, {500, 0.27, 3},
                            {600, 0.38, 4}, {700, 0.37, 4}}, 0.42, 4);
    }

    Result fromEffectiveShear(double shear) {
        if (shear <= -9998.0) return {};
        if (shear == 0.0) return {0.0, 0};
        return lookup(shear, {{20, 0.03, 0}, {30, 0.05, 0}, {40, 0.06, 0}, {50, 0.12, 1}, {60, 0.19, 3}, {70, 0.27, 3}, {80, 0.36, 4}}, 0.26, 3);
    }

    Result fromStpEffective(double stp) {
        return lookup(stp, {{0.1, 0.06, 0}, {0.5, 0.08, 1}, {1.0, 0.12, 1}, {2.0, 0.17, 2}, {4.0, 0.25, 3}, {6.0, 0.32, 4}, {8.0, 0.34, 4},
                            {10.0, 0.55, 5}}, 0.58, 5);
    }

    Result fromStpFixed(double stp) {
        return lookup(stp, {{0.1, 0.05, 0}, {0.5, 0.06, 0}, {1.0, 0.11, 1}, {2.0, 0.17, 2}, {3.0, 0.25, 3}, {5.0, 0.25, 3}, {7.0, 0.39, 4},
                            {9.0, 0.55, 5}}, 0.59, 5);
    }
}
