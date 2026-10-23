// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSUN_H
#define UTILITYSUN_H

#include <QByteArray>
#include <QDateTime>
#include <QImage>
#include <QPointF>
#include <QString>
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
        int cProbability{0};
        int mProbability{0};
        int xProbability{0};
        int protonProbability{0};
        std::string location;       // as SWPC lists it ("N13W42")
        int cEvents{0}, mEvents{0}, xEvents{0};   // flares of the last day
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
    // where a region was drawn on the picture (for a hover)
    struct Placed {
        Region region;
        QPointF position;           // pixels of the picture
        double longitude{0.0};      // west of the central meridian now
        bool behind{false};         // just over the limb
    };
    // the picture, and where each region is on it
    QImage labelled(const QImage& disc, const std::vector<Region>& regions, const QDateTime& when, int width, std::vector<Placed> * placed = nullptr);
    // the same, read from SWPC: the regions and the picture are returned too
    QImage labelled(int width, std::vector<Placed>& placed, QDateTime * time = nullptr);
    // what to say of a region on hover
    QString describe(const Placed&);
}

#endif  // UTILITYSUN_H
