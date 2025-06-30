/* Geodesy by Mike Gavaghan
 *
 * http://www.gavaghan.org/blog/free-source-code/geodesy-library-vincentys-formula/
 *
 * This code may be freely used and modified on any personal or professional
 * project. It comes with no warranty.
 *
 * BitCoin tips graciously accepted at 1FB63FYQMy7hpC2ANVhZ5mSgAZEtY1aVLf
 */

// Utility methods for dealing with angles.
// @author Mike Gavaghan

#include "ExternalAngle.h"
#include <numbers>
#include <cmath>

// Degrees/Radians conversion constant.
const double ExternalAngle::piOver180{std::numbers::pi / 180.0};

// Convert degrees to radians.
double ExternalAngle::toRadians(double degrees) {
    return degrees * piOver180;
}

// Convert radians to degrees.
double ExternalAngle::toDegrees(double radians) {
    return radians / piOver180;
}
