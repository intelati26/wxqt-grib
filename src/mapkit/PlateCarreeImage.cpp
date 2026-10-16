// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/PlateCarreeImage.h"
#include <algorithm>

void PlateCarreeImage::paint(QPainter& painter, MapHost& host, const QImage& image, double west, double south, double east, double north, double opacity, bool smooth) {
    if (image.isNull()) {
        return;
    }
    const auto t = host.view().transform();
    const auto * device = painter.device();
    QImage buffer{device->width(), device->height(), QImage::Format_ARGB32_Premultiplied};
    buffer.fill(Qt::transparent);
    {
        QPainter strip{&buffer};
        strip.setWindow(painter.window());      // the map is painted in window units
        strip.setViewport(painter.viewport());
        strip.setWorldTransform(painter.worldTransform());
        strip.setCompositionMode(QPainter::CompositionMode_Source);
        strip.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
        const int rows = image.height();
        const int strips = std::clamp(rows / 2, 1, 80);
        for (int s = 0; s < strips; s++) {
            const int r0 = rows * s / strips, r1 = rows * (s + 1) / strips;
            const double lat0 = north - (north - south) * r0 / rows, lat1 = north - (north - south) * r1 / rows;
            const QPointF a = t(lat0, west), b = t(lat1, east);
            strip.drawImage(QRectF{a, b}.normalized().adjusted(0.0, -0.3, 0.0, 0.3), image, QRectF{0.0, static_cast<double>(r0), static_cast<double>(image.width()), static_cast<double>(r1 - r0)});
        }
    }
    painter.save();
    painter.resetTransform();
    painter.setWindow(0, 0, buffer.width(), buffer.height());
    painter.setViewport(0, 0, buffer.width(), buffer.height());
    painter.setOpacity(opacity);
    painter.drawImage(0, 0, buffer);
    painter.restore();
}
