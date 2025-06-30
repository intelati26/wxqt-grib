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

#ifndef EXTERNALANGLE_H
#define EXTERNALANGLE_H

class ExternalAngle {
public:
    static const double piOver180;
    static double toRadians(double);
    static double toDegrees(double);
};

#endif  // EXTERNALANGLE_H
