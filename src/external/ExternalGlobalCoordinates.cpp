/* Geodesy by Mike Gavaghan
 *
 * http://www.gavaghan.org/blog/free-source-code/geodesy-library-vincentys-formula/
 *
 * This code may be freely used and modified on any personal or professional
 * project.  It comes with no warranty.
 *
 * BitCoin tips graciously accepted at 1FB63FYQMy7hpC2ANVhZ5mSgAZEtY1aVLf
*/

//   <p>
//   Encapsulation of latitude and longitude coordinates on a globe. Negative
//  latitude is southern hemisphere. Negative longitude is western hemisphere.
//   </p>
//   <p>
//   Any angle may be specified for longtiude and latitude, but all angles will be
//   canonicalized such that:
//   </p>
//
//   <pre>
//   -90 &lt= latitude &lt= +90 - 180 &lt longitude &lt= +180
//   </pre>
//
//   @author Mike Gavaghan

#include "ExternalGlobalCoordinates.h"

ExternalGlobalCoordinates::ExternalGlobalCoordinates(double latitude, double longitude)
    : latitude{latitude}
    , longitude{longitude}
{}

ExternalGlobalCoordinates ExternalGlobalCoordinates::withEc(const ExternalGlobalCoordinates& ec, bool lonNegativeOne) {
    return lonNegativeOne ? ExternalGlobalCoordinates{ec.getLatitude(), ec.getLongitude() * -1.0} : ExternalGlobalCoordinates{ec.getLatitude(), ec.getLongitude()};
}

ExternalGlobalCoordinates ExternalGlobalCoordinates::withPn(const ProjectionNumbers& pn, bool lonNegativeOne) {
    return lonNegativeOne ? ExternalGlobalCoordinates{pn.getLatLon().lat(), pn.getLatLon().lon() * -1.0} : ExternalGlobalCoordinates{pn.getLatLon().lat(), pn.getLatLon().lon()};
}

double ExternalGlobalCoordinates::getLatitude() const {
    return latitude;
}

double ExternalGlobalCoordinates::getLongitude() const {
    return longitude;
}
