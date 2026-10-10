// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYTROPICALALERTS_H
#define UTILITYTROPICALALERTS_H

#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// The hurricane and tropical storm watches and warnings that reach inland, as the National Weather Service issues them for its forecast zones
// and counties (api.weather.gov alerts, GeoJSON): NHC's own files give only the lines along the coast. Each alert is one zone's area.
class UtilityTropicalAlerts {
public:
    using Ring = vector<std::pair<double, double>>;     // (lon, lat)
    struct Area {
        string event;           // "Tropical Storm Warning"
        string code;            // HWR, HWA, TWR, TWA, SSW, SSA (as UtilityNhcGis::colorFor takes)
        string zone;            // "Calhoun"
        string office;          // the issuing forecast office, "NWS Tallahassee FL"
        string expires;         // ISO time
        vector<Ring> rings;     // the outer rings of the zone's polygon(s)
        double lat{0.0};        // the middle of its bounding box
        double lon{0.0};
    };
    // the alerts of /alerts/active?event=... (a FeatureCollection); those without a geometry or of another kind are left out
    static vector<Area> parse(const string& geojson);
    static string codeFor(const string& event);   // "" when it is not one of the six
    static int rank(const string& code);          // drawing order: the watches first, the warnings over them
};

#endif  // UTILITYTROPICALALERTS_H
