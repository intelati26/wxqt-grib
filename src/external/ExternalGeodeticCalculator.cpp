/* Geodesy by Mike Gavaghan
 *
 * http://www.gavaghan.org/blog/free-source-code/geodesy-library-vincentys-formula/
 *
 * This code may be freely used and modified on any personal or professional
 * project. It comes with no warranty.
 *
 * BitCoin tips graciously accepted at 1FB63FYQMy7hpC2ANVhZ5mSgAZEtY1aVLf
 */

/*
    <p>
    Implementation of Thaddeus Vincenty's algorithms to solve the direct and
    inverse geodetic problems. For more information, see Vincent's original
    publication on the NOAA website:
    </p>
    See http://www.ngs.noaa.gov/PUBS_LIB/inverse.pdf

    @author Mike Gavaghan
 */

#include "ExternalGeodeticCalculator.h"
#include <cmath>
#include "external/ExternalAngle.h"

// Calculate the destination and const double bearing after traveling a specified
// distance, and a specified starting bearing, for an initial location. This
// is the solution to the direct geodetic problem.
//
// @param ellipsoid reference ellipsoid to use
// @param start - starting location
// @param startBearing starting bearing (degrees)
// @param distance distance to travel (meters)
// @param endBearing bearing at destination (degrees) element at index 0 will
//                        be populated with the result
// @return
ExternalGlobalCoordinates ExternalGeodeticCalculator::calculateEndingGlobalCoordinates(ExternalGlobalCoordinates start, double startBearing, double distance) {
    vector<double> dlist{0.0, 0.0};
    return calculateEndingGlobalCoordinatesOriginal(ExternalEllipsoid::wgs84(), start, startBearing, distance, &dlist);
}

ExternalGlobalCoordinates ExternalGeodeticCalculator::calculateEndingGlobalCoordinatesOriginal(
        ExternalEllipsoid ellipsoid,
        ExternalGlobalCoordinates start,
        double startBearing,
        double distance,
        vector<double> * endBearing) {
    const auto a = ellipsoid.getSemiMajorAxis();
    const auto b = ellipsoid.getSemiMinorAxis();
    const auto aSquared = a * a;
    const auto bSquared = b * b;
    const auto f = ellipsoid.getFlattening();
    const auto phi1 = ExternalAngle::toRadians(start.getLatitude());
    const auto alpha1 = ExternalAngle::toRadians(startBearing);
    const auto cosAlpha1 = cos(alpha1);
    const auto sinAlpha1 = sin(alpha1);
    const auto s = distance;
    const auto tanU1 = (1.0 - f) * tan(phi1);
    const auto cosU1 = 1.0 / sqrt(1.0 + tanU1 * tanU1);
    const auto sinU1 = tanU1 * cosU1;
    // eq. 1
    const auto sigma1 = atan2(tanU1, cosAlpha1);
    // eq. 2
    const auto sinAlpha = cosU1 * sinAlpha1;
    const auto sin2Alpha = sinAlpha * sinAlpha;
    const auto cos2Alpha = 1.0 - sin2Alpha;
    const auto uSquared = cos2Alpha * (aSquared - bSquared) / bSquared;
    // eq. 3
    const auto A = 1.0 + (uSquared / 16384.0) * (4096.0 + uSquared * (-768.0 + uSquared * (320.0 - 175.0 * uSquared)));
    // eq. 4
    const auto B = (uSquared / 1024.0) * (256.0 + uSquared * (-128.0 + uSquared * (74.0 - 47.0 * uSquared)));
    // iterate until there is a negligible change in sigma
    const auto sOverbA = s / (b * A);
    auto sigma = sOverbA;
    double sinSigma;
    auto prevSigma = sOverbA;
    double sigmaM2;
    double cosSigmaM2;
    double cos2SigmaM2;
    for (;;) {
        // eq. 5
        sigmaM2 = 2.0 * sigma1 + sigma;
        cosSigmaM2 = cos(sigmaM2);
        cos2SigmaM2 = cosSigmaM2 * cosSigmaM2;
        sinSigma = sin(sigma);
        const auto cosSignma = cos(sigma);
        // eq. 6
        auto deltaSigma = B * sinSigma * (cosSigmaM2 + (B / 4.0) * (cosSignma * (-1.0 + 2.0 * cos2SigmaM2) - (B / 6.0) * cosSigmaM2 * (-3.0 + 4.0 * sinSigma * sinSigma) * (-3.0 + 4.0 * cos2SigmaM2)));
        // eq. 7
        sigma = sOverbA + deltaSigma;
        // break after converging to tolerance
        if (abs(sigma - prevSigma) < 0.0000000000001)
            break;

        prevSigma = sigma;
    }
    sigmaM2 = 2.0 * sigma1 + sigma;
    cosSigmaM2 = cos(sigmaM2);
    cos2SigmaM2 = cosSigmaM2 * cosSigmaM2;
    const auto cosSigma = cos(sigma);
    sinSigma = sin(sigma);
    // eq. 8
    const auto phi2 = atan2(sinU1 * cosSigma + cosU1 * sinSigma * cosAlpha1, (1.0 - f) * sqrt(sin2Alpha + pow(sinU1 * sinSigma - cosU1 * cosSigma * cosAlpha1, 2.0)));
    // eq. 9
    // This fixes the pole crossing defect spotted by Matt Feemster. When a
    // path passes a pole and essentially crosses a line of latitude twice -
    // once in each direction - the longitude calculation got messed up. Using
    // atan2 instead of atan fixes the defect. The change is in the next 3
    // lines.
    // double tanLambda = sinSigma * sinAlpha1 / (cosU1 * cosSigma - sinU1 * sinSigma * cosAlpha1);
    // double lambda = Math.atan(tanLambda);
    const auto lambda = atan2(sinSigma * sinAlpha1, (cosU1 * cosSigma - sinU1 * sinSigma * cosAlpha1));
    // eq. 10
    const auto C = (f / 16.0) * cos2Alpha * (4.0 + f * (4.0 - 3.0 * cos2Alpha));
    // eq. 11
    const auto L = lambda - (1.0 - C) * f * sinAlpha * (sigma + C * sinSigma * (cosSigmaM2 + C * cosSigma * (-1.0 + 2.0 * cos2SigmaM2)));
    // eq. 12
    const auto alpha2 = atan2(sinAlpha, -sinU1 * sinSigma + cosU1 * cosSigma * cosAlpha1);
    // build result
    const auto latitude = ExternalAngle::toDegrees(phi2);
    const auto longitude = start.getLongitude() + ExternalAngle::toDegrees(L);
    if (endBearing != nullptr && !endBearing->empty()) {
        (*endBearing)[0] = ExternalAngle::toDegrees(alpha2);
    }
    return {latitude, longitude};
}
