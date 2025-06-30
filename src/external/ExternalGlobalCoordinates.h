/* Geodesy by Mike Gavaghan
 *
 * http://www.gavaghan.org/blog/free-source-code/geodesy-library-vincentys-formula/
 *
 * This code may be freely used and modified on any personal or professional
 * project.  It comes with no warranty.
 *
 * BitCoin tips graciously accepted at 1FB63FYQMy7hpC2ANVhZ5mSgAZEtY1aVLf
 */

/*
<p>
Encapsulation of latitude and longitude coordinates on a globe. Negative
latitude is southern hemisphere. Negative longitude is western hemisphere.
</p>
<p>
Any angle may be specified for longitude and latitude, but all angles will be
canonicalized such that:
</p>

<pre>
-90 &lt= latitude &lt= +90 - 180 &lt longitude &lt= +180
</pre>

@author Mike Gavaghan
 */

#ifndef EXTERNALGLOBALCOORDINATES_H
#define EXTERNALGLOBALCOORDINATES_H

#include "radar/ProjectionNumbers.h"

class ExternalGlobalCoordinates {
public:
    ExternalGlobalCoordinates(double, double);
    static ExternalGlobalCoordinates withEc(const ExternalGlobalCoordinates&, bool);
    static ExternalGlobalCoordinates withPn(const ProjectionNumbers&, bool);
    double getLatitude() const;
    double getLongitude() const;

private:
    // Latitude in degrees. Negative latitude is southern hemisphere. */
    double latitude{};
    // Longitude in degrees. Negative longitude is western hemisphere. */
    double longitude{};
};

#endif  // EXTERNALGLOBALCOORDINATES_H
