// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGTHERMO_H
#define SOUNDINGTHERMO_H

// Atmospheric thermodynamics for sounding analysis. Pure C++ (no Qt) so it can be
// unit-tested on its own. Pressure is in millibars, temperature in degrees C,
// mixing ratio in g/kg, heights in metres. The formulations (Wobus moist-adiabat
// polynomial, Bolton-style LCL, the saturation-vapour-pressure polynomial) are the
// ones used by the NWS/SPC sounding programs, so results can be compared to SPC's
// published sounding output number for number.
namespace SoundingThermo {
    constexpr double missing = -9999.0;
    constexpr double zeroCelsiusK = 273.15;
    constexpr double gravity = 9.80665;
    constexpr double rocp = 0.28571426;   // R / Cp for dry air

    inline bool isMissing(double value) { return value <= -9998.0; }

    double vaporPressure(double tC);                        // saturation, mb
    double mixingRatio(double pMb, double tC);              // saturation at tC, g/kg
    double virtualTemp(double pMb, double tC, double tdC);  // C; tdC selects the vapour content
    double theta(double pMb, double tC, double p2Mb = 1000.0);   // C
    double thetaE(double pMb, double tC, double tdC);       // C
    double lclTemp(double tC, double tdC);                  // C
    void dryLift(double pMb, double tC, double tdC, double& lclPres, double& lclTemp);
    double wobus(double tC);
    double satLift(double pMb, double thetaM);              // C
    double wetLift(double pMb, double tC, double p2Mb);     // C: lift moist-adiabatically from pMb to p2Mb
    double wetBulb(double pMb, double tC, double tdC);      // C
    double tempAtMixingRatio(double wGkg, double pMb);      // dewpoint C for mixing ratio w at p
    double relativeHumidityPct(double pMb, double tC, double tdC);
}

#endif  // SOUNDINGTHERMO_H
