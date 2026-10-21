// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SATELLITEORBIT_H
#define SATELLITEORBIT_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// Where a low earth orbit satellite is, from its two line element set (the public ones of CelesTrak): the SGP4 model of Spacetrack Report No. 3 (Hoots and Roehrich, 1980) for orbits of under
// 225 minutes, which every weather and radar satellite here has; and the passes over a place. Pure arithmetic, no network.
namespace SatelliteOrbit {
    struct Tle {
        string name;
        int catalog{0};
        double epochJd{0.0};          // Julian date (UTC) of the elements
        double bstar{0.0};
        double inclination{0.0};      // radians
        double node{0.0};             // right ascension of the ascending node, radians
        double eccentricity{0.0};
        double argPerigee{0.0};       // radians
        double meanAnomaly{0.0};      // radians
        double meanMotion{0.0};       // radians per minute (Kozai)
        bool valid{false};
    };
    // the two lines (and the name line before them, when there is one)
    Tle parse(const string& name, const string& line1, const string& line2);
    // a text of three-line sets (name, line 1, line 2), as CelesTrak gives them
    vector<Tle> parseAll(const string& text);

    struct State {
        double x, y, z;               // km, TEME (the inertial frame of the elements)
        double vx, vy, vz;            // km/s
        bool ok{false};
    };
    class Propagator {
    public:
        explicit Propagator(const Tle& tle);
        State at(double julianDate) const;   // UTC
        const Tle& tle() const { return elements; }
        bool deepSpace() const { return period >= 225.0; }

    private:
        Tle elements;
        double period{0.0};   // minutes
        bool simple{false};
        double aodp{0}, xnodp{0}, cosio{0}, sinio{0}, theta2{0}, x3thm1{0}, x1mth2{0}, x7thm1{0};
        double eta{0}, c1{0}, c4{0}, c5{0}, d2{0}, d3{0}, d4{0}, t2cof{0}, t3cof{0}, t4cof{0}, t5cof{0};
        double xmdot{0}, omgdot{0}, xnodot{0}, xnodcf{0}, omgcof{0}, xmcof{0}, xlcof{0}, aycof{0}, delmo{0}, sinmo{0};
    };

    // the point on the ground under it
    struct Ground {
        double lat, lon;              // degrees, east positive
        double altitudeKm;
    };
    Ground groundOf(const State& s, double julianDate);
    double julianDate(long long unixSeconds);
    long long unixSeconds(double julianDate);

    // A sensor's swath: kilometers either side of the ground track, with a gap under the satellite (ASCAT) of innerKm either side.
    struct Sensor {
        string label;
        double halfWidthKm;
        double innerKm{0.0};
    };
    struct Pass {
        long long time{0};            // unix seconds of the closest approach
        double distanceKm{0.0};       // from the place to the ground track, then (signed: negative to the left of the track)
        bool inSwath{false};          // the place is within the sensor's swath at that moment
        bool ascending{false};        // moving north
        double satLat{0.0}, satLon{0.0};   // under the satellite then
        double sunElevation{0.0};     // of the sun over the place then, degrees
    };
    // the passes of one satellite over a place in the next `hours` whose swath covers it (the sensor looks across its swath at the closest approach: that is the time of the pass)
    vector<Pass> passes(const Propagator& sat, double lat, double lon, long long fromUnix, double hours, const Sensor& sensor);
    // the ground track: unix seconds and the point, every `stepSeconds`
    struct TrackPoint {
        long long time;
        double lat, lon;
        bool ascending;
    };
    vector<TrackPoint> track(const Propagator& sat, long long fromUnix, double seconds, double stepSeconds);
    double sunElevation(double lat, double lon, long long unixSeconds);
    // the edge of the swath (km either side, signed) at a point of the track
    void swathEdge(const Propagator& sat, long long unixSeconds, double offsetKm, double& lat, double& lon);
}

#endif  // SATELLITEORBIT_H
