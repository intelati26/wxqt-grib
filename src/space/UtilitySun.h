// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSUN_H
#define UTILITYSUN_H

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <string>
#include <vector>

// The Sun as SDO's HMI sees it (NOAA SWPC's picture of the visible disc), with a heliographic grid and the numbered active regions of SWPC's region list drawn on it: east (left) and west (right)
// limbs, the equator and meridians every 30 degrees as the tilt of the Sun's axis shows them, each region's number where it is, coloured by its chance of an M or X flare. Solar north is up.
namespace UtilitySun {
    struct Region {
        int number{0};
        double latitude{0.0};
        double longitude{0.0};      // degrees west of the central meridian (Stonyhurst)
        int area{0};                // millionths of the disc, -1 when not given
        std::string spotClass;
        std::string magClass;
        int spots{0};
        int mProbability{0};
        int xProbability{0};
        QDate observed;
    };
    // the regions of the newest day of the list (JSON text from SWPC's solar_regions.json)
    std::vector<Region> parseRegions(const QByteArray& json);
    // B0, the tilt of the Sun's north pole toward the Earth, in degrees, for a time
    double tilt(const QDateTime& utc);
    // degrees a region at this latitude has moved west (Stonyhurst) in `days`
    double drift(double latitude, double days);
    // where the point is on the disc: x to the west (right) and y to the north (up), in solar radii; false when it is on the far side
    bool onDisc(double latitude, double longitudeWest, double b0, double& x, double& y);
    // the picture, drawn `width` pixels wide; null when the Sun's picture or the list could not be read
    QImage labelled(int width, QDateTime * time = nullptr);
    QImage labelled(const QImage& disc, const std::vector<Region>& regions, const QDateTime& when, int width);
}

#endif  // UTILITYSUN_H
