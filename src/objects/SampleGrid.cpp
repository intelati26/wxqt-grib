// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/SampleGrid.h"
#include <cmath>
#include <QFile>
#include <QTextStream>

SampleGrid SampleGrid::load(const QString& path) {
    SampleGrid grid;
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return grid;
    }
    std::vector<double> lons;
    std::vector<double> lats;
    std::vector<float> values;
    QTextStream in{&file};
    while (!in.atEnd()) {
        const auto parts = in.readLine().split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 3) {
            continue;
        }
        lons.push_back(parts[0].toDouble());
        lats.push_back(parts[1].toDouble());
        values.push_back(parts[2].toFloat());
    }
    if (values.size() < 4) {
        return grid;
    }
    int cols = 1;
    for (size_t i = 1; i < lons.size(); i += 1) {
        if (lons[i] < lons[i - 1]) {
            break;
        }
        cols += 1;
    }
    if (cols < 2 || static_cast<int>(values.size()) < cols * 2) {
        return grid;
    }
    grid.cols = cols;
    grid.rows = static_cast<int>(values.size()) / cols;
    grid.x0 = lons[0];
    grid.dx = lons[1] - lons[0];
    grid.y0 = lats[0];
    grid.dy = lats[static_cast<size_t>(cols)] - lats[0];   // negative
    grid.values = std::move(values);
    return grid;
}

bool SampleGrid::valueAt(double lon, double lat, double& out) const {
    if (cols < 2 || rows < 1 || dx == 0.0 || dy == 0.0) {
        return false;
    }
    const auto col = static_cast<int>(std::lround((lon - x0) / dx));
    const auto row = static_cast<int>(std::lround((lat - y0) / dy));
    if (col < 0 || col >= cols || row < 0 || row >= rows) {
        return false;
    }
    const auto value = values[static_cast<size_t>(row) * cols + col];
    if (!std::isfinite(value) || value < -9000.0F) {
        return false;
    }
    out = value;
    return true;
}

bool SampleGrid::snap(double fx, double fy, double& lon, double& lat, double& markerFx, double& markerFy) const {
    if (cols < 2 || rows < 1) {
        return false;
    }
    // grid coords are cell centres; the PNG this backs shares the warp extent
    const auto west = x0 - dx / 2.0;
    const auto east = x0 + (cols - 0.5) * dx;
    const auto north = y0 - dy / 2.0;
    const auto south = y0 + (rows - 0.5) * dy;

    const auto cursorLon = west + fx * (east - west);
    const auto cursorLat = north + fy * (south - north);
    const auto col = static_cast<int>(std::lround((cursorLon - x0) / dx));
    const auto row = static_cast<int>(std::lround((cursorLat - y0) / dy));
    if (col < 0 || col >= cols || row < 0 || row >= rows) {
        return false;
    }
    lon = x0 + col * dx;
    lat = y0 + row * dy;
    markerFx = (lon - west) / (east - west);
    markerFy = (lat - north) / (south - north);
    return true;
}
