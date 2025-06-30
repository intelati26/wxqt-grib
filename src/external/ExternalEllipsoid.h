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

#ifndef EXTERNALELLIPSOID_H
#define EXTERNALELLIPSOID_H

class ExternalEllipsoid {
public:
    double getSemiMajorAxis() const;
    double getSemiMinorAxis() const;
    double getFlattening() const;
    static ExternalEllipsoid wgs84();
    static ExternalEllipsoid fromAAndInverseF(double, double);

private:
    ExternalEllipsoid(double, double, double);
    double semiMajor;
    double semiMinor;
    double flattening;
};

#endif  // EXTERNALELLIPSOID_H
