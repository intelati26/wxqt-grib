// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingThermo.h"
#include <cmath>

namespace SoundingThermo {

// Wobus polynomial fit of saturation vapour pressure (mb) over water
double vaporPressure(double tC) {
    double pol = tC * (1.1112018e-17 + tC * (-3.0994571e-20));
    pol = tC * (2.1874425e-13 + tC * (-1.789232e-15 + pol));
    pol = tC * (4.3884180e-09 + tC * (-2.988388e-11 + pol));
    pol = tC * (7.8736169e-05 + tC * (-6.111796e-07 + pol));
    pol = 0.99999683 + tC * (-9.082695e-03 + pol);
    return 6.1078 / std::pow(pol, 8);
}

double mixingRatio(double pMb, double tC) {
    const double x = 0.02 * (tC - 12.5 + 7500.0 / pMb);
    const double wfw = 1.0 + 0.0000045 * pMb + 0.0014 * x * x;
    const double fwesw = wfw * vaporPressure(tC);
    return 621.97 * (fwesw / (pMb - fwesw));
}

double virtualTemp(double pMb, double tC, double tdC) {
    const double tk = tC + zeroCelsiusK;
    const double w = 0.001 * mixingRatio(pMb, tdC);
    return tk * (1.0 + w / 0.622) / (1.0 + w) - zeroCelsiusK;
}

double theta(double pMb, double tC, double p2Mb) {
    return (tC + zeroCelsiusK) * std::pow(p2Mb / pMb, rocp) - zeroCelsiusK;
}

double lclTemp(double tC, double tdC) {
    const double s = tC - tdC;
    const double dlt = s * (1.2185 + 0.001278 * tC + s * (-0.00219 + 1.173e-5 * s - 0.0000052 * tC));
    return tC - dlt;
}

void dryLift(double pMb, double tC, double tdC, double& lclPres, double& lclTempOut) {
    lclTempOut = lclTemp(tC, tdC);
    lclPres = 1000.0 * std::pow((lclTempOut + zeroCelsiusK) / (theta(pMb, tC, 1000.0) + zeroCelsiusK), 1.0 / rocp);
}

double wobus(double tC) {
    const double t = tC - 20.0;
    if (t <= 0.0) {
        double npol = 1.0 + t * (-8.841660499999999e-3 + t * (1.4714143e-4 + t * (-9.671989000000001e-7 +
                      t * (-3.2607217e-8 + t * (-3.8598073e-10)))));
        npol = 15.13 / std::pow(npol, 4);
        return npol;
    }
    double ppol = t * (4.9618922e-07 + t * (-6.1059365e-09 + t * (3.9401551e-11 + t * (-1.2588129e-13 +
                  t * (1.6688280e-16)))));
    ppol = 1.0 + t * (3.6182989e-03 + t * (-1.3603273e-05 + ppol));
    return (29.93 / std::pow(ppol, 4)) + (0.96 * t) - 14.8;
}

// temperature at pMb on the moist adiabat with wet-bulb potential temperature thetaM
double satLift(double pMb, double thetaM) {
    if (std::fabs(pMb - 1000.0) - 0.001 <= 0.0) {
        return thetaM;
    }
    double eor = 999.0;
    double rate = 1.0;
    double t1 = 0.0;
    double t2 = 0.0;
    double e1 = 0.0;
    double e2 = 0.0;
    const double pwrp = std::pow(pMb / 1000.0, rocp);
    int guard = 0;
    while (std::fabs(eor) - 0.1 > 0.0 && guard < 200) {
        guard += 1;
        if (eor == 999.0) {
            t1 = (thetaM + zeroCelsiusK) * pwrp - zeroCelsiusK;
            e1 = wobus(t1) - wobus(thetaM);
            rate = 1.0;
        } else {
            rate = (t2 - t1) / (e2 - e1);
            t1 = t2;
            e1 = e2;
        }
        t2 = t1 - e1 * rate;
        e2 = (t2 + zeroCelsiusK) / pwrp - zeroCelsiusK;
        e2 += wobus(t2) - wobus(e2) - thetaM;
        eor = e2 * rate;
    }
    return t2 - eor;
}

double wetLift(double pMb, double tC, double p2Mb) {
    const double thta = theta(pMb, tC, 1000.0);
    const double thetam = thta - wobus(thta) + wobus(tC);
    return satLift(p2Mb, thetam);
}

double wetBulb(double pMb, double tC, double tdC) {
    double mp = 0.0;
    double mt = 0.0;
    dryLift(pMb, tC, tdC, mp, mt);
    return wetLift(mp, mt, pMb);
}

// equivalent potential temperature: lift to the LCL, follow the moist adiabat to
// 100 mb (where essentially all vapour has condensed out), then dry-descend to 1000 mb
double thetaE(double pMb, double tC, double tdC) {
    double lclP = 0.0;
    double lclT = 0.0;
    dryLift(pMb, tC, tdC, lclP, lclT);
    return theta(100.0, wetLift(lclP, lclT, 100.0), 1000.0);
}

double tempAtMixingRatio(double w, double pMb) {
    const double c1 = 0.0498646455;
    const double c2 = 2.4082965;
    const double c3 = 7.07475;
    const double c4 = 38.9114;
    const double c5 = 0.0915;
    const double c6 = 1.2035;
    const double x = std::log10(w * pMb / (622.0 + w));
    const double a = std::pow(10.0, c5 * x) - c6;
    return (std::pow(10.0, c1 * x + c2) - c3 + c4 * a * a) - zeroCelsiusK;
}

double relativeHumidityPct(double pMb, double tC, double tdC) {
    return 100.0 * mixingRatio(pMb, tdC) / mixingRatio(pMb, tC);
}

}  // namespace SoundingThermo
