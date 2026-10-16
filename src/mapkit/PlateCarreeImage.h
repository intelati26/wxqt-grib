// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PLATECARREEIMAGE_H
#define PLATECARREEIMAGE_H

#include <QImage>
#include <QPainter>
#include "mapkit/MapLayer.h"

// An image on a latitude / longitude grid (each pixel the same number of degrees), laid over the Mercator master map: drawn in strips of rows, each straight between its own latitudes,
// into one buffer first (the strips overlap a hair so that no seam shows) and then over the map once at the opacity asked for. The top row of the image is the northern one.
namespace PlateCarreeImage {
    void paint(QPainter& painter, MapHost& host, const QImage& image, double west, double south, double east, double north, double opacity, bool smooth = false);
}

#endif  // PLATECARREEIMAGE_H
