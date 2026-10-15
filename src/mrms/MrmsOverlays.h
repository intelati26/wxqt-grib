// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSOVERLAYS_H
#define MRMSOVERLAYS_H

#include <utility>
#include <vector>
#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QRgb>
#include <QVector>
#include "mrms/UtilityMrms.h"

// The MRMS overlays that go over any product: lightning strikes, the lightning chance as contours, hail swaths and rotation tracks. Each function reads and
// decodes from the open data server (blocking: call it from a background task) and returns data only, so the MRMS screen and the master map draw the same things.
namespace MrmsOverlays {
    struct Strike {
        float lon, lat;
        unsigned char age;   // 0 the last minute, 1 within 5, 2 within 15, 3 within 30 (the shortest period the cell is in)
    };
    struct Lightning {
        std::vector<Strike> points;
        QDateTime utc;
        bool ok{false};
    };
    // the cloud-to-ground strikes of the last 1, 5, 15 and 30 minutes as far as `periods` (1 to 4) goes: each cell with a strike, as a point of the shortest period it is in
    Lightning lightning(int periods);

    struct ChanceLine {   // one contour segment: the level (percent) and its two ends
        unsigned char level;
        float lon1, lat1, lon2, lat2;
    };
    struct ChanceCell {   // one 5 km block with a chance of 10 percent or more: its centre, its size in degrees and the chance
        float lon, lat, size;
        unsigned char percent;
    };
    struct Chance {
        std::vector<ChanceLine> lines;
        std::vector<ChanceCell> cells;   // for the shaded bands under the lines
        bool ok{false};
    };
    // the chance of lightning in the next 30 (which 1) or 60 (2) minutes, contoured at 10, 30, 50 and 70 percent
    Chance chance(int which);

    struct Band {
        double from;
        QColor color;
        const char * label;
    };
    const std::vector<Band>& hailBands();       // MESH, mm from: 1 in and up
    const std::vector<Band>& rotationBands();   // azimuthal shear, 1e-3 / s from: 8 and up

    struct Swath {
        std::vector<std::pair<UtilityMrms::Grid, QByteArray>> levels;   // the cells (0 transparent) at the finest grid, then 4 x 4 and 16 x 16 cell blocks (the largest value of each)
        QVector<QRgb> colors;                                           // by cell index, premultiplied
        QDateTime utc;
        bool ok{false};
    };
    // the largest hail size of a period: which 1 .. 5 is the last 30 minutes, 1 hour, 2, 6 and 24 hours
    Swath hail(int which);
    // where the strongest rotation passed in a period: which 1 .. 5 the 0-2 km track of the last 30 min, 1 h, 2 h, 6 h, 24 h; 6 .. 8 the 3-6 km track of 1 h, 2 h, 6 h
    // (cleaned of the weak shear, the radars' poor quality areas and the other side of the border)
    Swath rotation(int which);
}

#endif  // MRMSOVERLAYS_H
