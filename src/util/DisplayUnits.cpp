// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/DisplayUnits.h"
#include "settings/UIPreferences.h"

bool DisplayUnits::us() {
    return UIPreferences::unitsF;
}

double DisplayUnits::value(double v, const std::string& nativeUnits, Kind kind) {
    if (nativeUnits == "gpm") {   // a height is in decameters, as on every chart of the Model Viewer (the same in both systems)
        return v / 10.0;
    }
    if (!us()) {
        return v;
    }
    if (nativeUnits == "deg C") {
        return v * 1.8 + (kind == Kind::Absolute ? 32.0 : 0.0);
    }
    if (nativeUnits == "mm" || nativeUnits == "mm/day") {
        return v / 25.4;
    }
    return v;
}

std::string DisplayUnits::label(const std::string& nativeUnits) {
    if (nativeUnits == "gpm") {
        return "dam";
    }
    if (!us()) {
        return nativeUnits;
    }
    if (nativeUnits == "deg C") {
        return "deg F";
    }
    if (nativeUnits == "mm") {
        return "in";
    }
    if (nativeUnits == "mm/day") {
        return "in/day";
    }
    return nativeUnits;
}
