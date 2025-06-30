/* Geodesy by Mike Gavaghan
 *
 * http://www.gavaghan.org/blog/free-source-code/geodesy-library-vincentys-formula/
 *
 * This code may be freely used and modified on any personal or professional
 * project. It comes with no warranty.
 *
 * BitCoin tips graciously accepted at 1FB63FYQMy7hpC2ANVhZ5mSgAZEtY1aVLf
 */

// Encapsulation of an ellipsoid, and declaration of common reference ellipsoids.
// @author Mike Gavaghan

#include "ExternalEllipsoid.h"

ExternalEllipsoid::ExternalEllipsoid(double semiMajor, double semiMinor, double flattening)
    : semiMajor{semiMajor}
    , semiMinor{semiMinor}
    , flattening{flattening}
{}

double ExternalEllipsoid::getSemiMajorAxis() const {
    return semiMajor;
}

double ExternalEllipsoid::getSemiMinorAxis() const {
    return semiMinor;
}

double ExternalEllipsoid::getFlattening() const {
    return flattening;
}

ExternalEllipsoid ExternalEllipsoid::wgs84() {
    return fromAAndInverseF(6378137.0, 298.257223563);
}

// Build an Ellipsoid from the semi major axis measurement and the inverse flattening.
ExternalEllipsoid ExternalEllipsoid::fromAAndInverseF(double semiMajor, double inverseFlattening) {
    const auto f = 1.0 / inverseFlattening;
    const auto b = (1.0 - f) * semiMajor;
    return {semiMajor, b, f};
}
