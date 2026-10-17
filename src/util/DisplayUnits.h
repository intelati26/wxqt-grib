// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DISPLAYUNITS_H
#define DISPLAYUNITS_H

#include <string>

// The units of the program's settings (UIPreferences::unitsF: Fahrenheit and inches, or Celsius and millimeters) for a value that arrives in metric units: one place for the factors, so that a
// map layer, a chart and a plume agree. Pressure (hPa) and height (gpm) are left as they are; a value of "standard deviations" is not a unit that changes.
namespace DisplayUnits {
    enum class Kind { Absolute, Delta };    // a temperature of 20 C is 68 F; a temperature DIFFERENCE of 20 C is 36 F
    bool us();                               // the setting
    double value(double v, const std::string& nativeUnits, Kind kind = Kind::Absolute);
    std::string label(const std::string& nativeUnits);   // "deg C" -> "deg F", "mm" -> "in", "mm/day" -> "in/day" (as the setting says), the others as they are
}

#endif  // DISPLAYUNITS_H
