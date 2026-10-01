// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingParcel.h"
#include <cmath>
#include <vector>
#include "sounding/SoundingThermo.h"

using SoundingThermo::gravity;
using SoundingThermo::isMissing;
using SoundingThermo::missing;

namespace SoundingParcel {

void mixedLayerStart(const SoundingProfile& prof, double depthMb, double& pres, double& temp, double& dwpt) {
    // Follows SHARPpy's "exact" layer mean (params.mean_theta / mean_mixratio): the layer ends
    // are interpolated, every observed level inside is used, and each counts equally. The
    // mixing ratio comes from the mean dewpoint at the mean pressure.
    const double pBot = prof.sfcPres();
    const double pTop = pBot - depthMb;
    pres = pBot;
    temp = dwpt = missing;
    const double tBot = prof.interpTemp(pBot), tTop = prof.interpTemp(pTop);
    const double dBot = prof.interpDwpt(pBot), dTop = prof.interpDwpt(pTop);
    if (isMissing(tBot) || isMissing(tTop) || isMissing(dBot) || isMissing(dTop)) {
        return;
    }
    double sumTheta = 0.0, sumTd = 0.0, sumP = 0.0;
    int nTheta = 0, nTd = 0;
    for (size_t i = 0; i < prof.size(); i += 1) {
        if (prof.pres[i] >= pBot || prof.pres[i] <= pTop) {
            continue;
        }
        if (!isMissing(prof.tmpc[i])) {
            sumTheta += SoundingThermo::theta(prof.pres[i], prof.tmpc[i], 1000.0);
            nTheta += 1;
        }
        if (!isMissing(prof.dwpc[i])) {
            sumTd += prof.dwpc[i];
            sumP += prof.pres[i];
            nTd += 1;
        }
    }
    const double meanTheta = (0.5 * (SoundingThermo::theta(pBot, tBot, 1000.0) + SoundingThermo::theta(pTop, tTop, 1000.0)) + sumTheta) / (nTheta + 1);
    const double meanTd = (0.5 * (dBot + dTop) + sumTd) / (nTd + 1);
    const double meanP = (0.5 * (pBot + pTop) + sumP) / (nTd + 1);
    temp = SoundingThermo::theta(1000.0, meanTheta, pBot);
    dwpt = SoundingThermo::tempAtMixingRatio(SoundingThermo::mixingRatio(meanP, meanTd), pBot);
}

namespace {
    using SoundingThermo::gravity;
    using SoundingThermo::virtualTemp;
    using SoundingThermo::wetLift;

    bool ok(double v) { return !isMissing(v); }
    double ctok(double c) { return c + SoundingThermo::zeroCelsiusK; }
}

// A port of SHARPpy's params.parcelx (Marsh, Hart, Halbert, Blumberg, Supinie et al.,
// BSD-3-Clause; see docs/sharppy-notice.md): one pass up the observed levels, summing
// the positive and negative layer energy, then the LFC/EL crossings.
Parcel liftFrom(const SoundingProfile& prof, double pres, double tmpc, double dwpc) {
    Parcel pcl;
    if (prof.size() < 3 || !ok(pres) || !ok(tmpc) || !ok(dwpc)) {
        return pcl;
    }
    pcl.lplPres = pres;
    pcl.lplTemp = tmpc;
    pcl.lplDwpt = dwpc;
    const double sfcP = prof.sfcPres();
    const double G = gravity;
    double pbot = std::fmin(sfcP, pres);

    double pe2 = 0.0, tp2 = 0.0;
    SoundingThermo::dryLift(pres, tmpc, dwpc, pe2, tp2);
    if (!ok(pe2) || std::isnan(pe2)) {
        return pcl;
    }
    const double blupper = pe2;
    const double h2lcl = prof.interpHght(pe2);
    if (!ok(h2lcl)) {
        return pcl;
    }
    pcl.lclPres = std::fmin(pe2, sfcP);
    pcl.lclHght = prof.toAgl(h2lcl);
    // dry ascent to the LCL, for the drawn trace
    const double thetaStart = SoundingThermo::theta(pres, tmpc, 1000.0);
    pcl.tracePres.push_back(pres);
    pcl.traceTemp.push_back(tmpc);
    for (double pp = std::floor(pres / 25.0) * 25.0; pp > pe2; pp -= 25.0) {
        if (pp < pres) {
            pcl.tracePres.push_back(pp);
            pcl.traceTemp.push_back(SoundingThermo::theta(1000.0, thetaStart, pp));
        }
    }
    pcl.tracePres.push_back(pe2);
    pcl.traceTemp.push_back(tp2);

    // ---- the dry layer below the LCL, on a 1 mb grid, compared in theta space ----
    const double thetaParcel = SoundingThermo::theta(pe2, tp2, 1000.0);
    const double blmr = SoundingThermo::mixingRatio(pres, dwpc);
    double totp = 0.0, totn = 0.0, tote = 0.0;
    double cinhAtLfc = 0.0;   // SHARPpy's cinh_old: the negative energy summed up to the last LFC
    {
        std::vector<double> tdef, hh;
        for (double pp = pbot; pp >= blupper - 1e-9; pp -= 1.0) {
            const double te = prof.interpTemp(pp), td = prof.interpDwpt(pp), h = prof.interpHght(pp);
            if (!ok(te) || !ok(td) || !ok(h)) {
                continue;
            }
            const double tvEnv = virtualTemp(pp, SoundingThermo::theta(pp, te, 1000.0), td);
            const double tvPcl = virtualTemp(pp, thetaParcel, SoundingThermo::tempAtMixingRatio(blmr, pp));
            tdef.push_back((tvPcl - tvEnv) / ctok(tvEnv));
            hh.push_back(h);
        }
        for (size_t i = 0; i + 1 < tdef.size(); i += 1) {
            const double lyre = G * (tdef[i] + tdef[i + 1]) / 2.0 * (hh[i + 1] - hh[i]);
            if (lyre < 0.0) {
                totn += lyre;
            }
        }
    }
    cinhAtLfc = totn;
    if (pbot > pe2) {
        pbot = pe2;
    }

    // ---- the moist ascent ----
    size_t lptr = prof.size();
    for (size_t i = 0; i < prof.size(); i += 1) {
        if (pbot >= prof.pres[i]) {
            lptr = i;
            break;
        }
    }
    if (lptr >= prof.size()) {
        return pcl;
    }
    double pe1 = pbot;
    double h1 = prof.interpHght(pe1);
    double te1 = prof.interpVtmp(pe1);
    double tp1 = wetLift(pe2, tp2, pe1);
    double lyre = 0.0, lyrlast = 0.0;
    double lfcP = missing, elP = missing, mplP = missing;
    bool haveElOrLfcReset = false;
    bool liDone5 = false, liDone3 = false;
    bool b3Set = false, b6Set = false;
    const bool lcl3 = pcl.lclHght < 3000.0;
    const bool lcl6 = pcl.lclHght < 6000.0;
    (void) haveElOrLfcReset;
    pcl.cape3km = 0.0;
    double b6km = 0.0;
    (void) b6km;
    const double pTopProfile = prof.pres.back();

    auto buoyant = [&] (double p, double pStart, double tStart, double envV) {
        const double tw = wetLift(pStart, tStart, p);
        return envV < virtualTemp(p, tw, tw);   // parcel warmer than environment
    };

    for (size_t i = lptr; i < prof.size(); i += 1) {
        if (!ok(prof.tmpc[i]) || !ok(prof.vtmp[i])) {
            continue;
        }
        const double pe2l = prof.pres[i];
        const double h2 = prof.hght[i];
        const double te2 = prof.vtmp[i];
        const double tp2l = wetLift(pe1, tp1, pe2l);
        pcl.tracePres.push_back(pe2l);
        pcl.traceTemp.push_back(tp2l);
        const double tdef1 = (virtualTemp(pe1, tp1, tp1) - te1) / ctok(te1);
        const double tdef2 = (virtualTemp(pe2l, tp2l, tp2l) - te2) / ctok(te2);
        lyrlast = lyre;
        lyre = G * (tdef1 + tdef2) / 2.0 * (h2 - h1);
        if (lyre > 0.0) {
            totp += lyre;
        } else if (pe2l > 500.0) {
            totn += lyre;
        }
        tote += lyre;
        const double pelast = pe1;
        pe1 = pe2l;
        te1 = te2;
        tp1 = tp2l;

        // energy within the lowest 3 km (only when the LCL is below 3 km)
        if (lcl3) {
            if (prof.toAgl(h1) <= 3000.0 && prof.toAgl(h2) >= 3000.0 && !b3Set) {
                const double pe3 = pelast;
                const double h3 = prof.interpHght(pe3);
                const double te3 = prof.interpVtmp(pe3);
                const double tp3 = wetLift(pe1, tp1, pe3);
                double b3 = lyre > 0.0 ? totp - lyre : totp;
                const double h4 = prof.toMsl(3000.0);
                const double pe4 = prof.interpPresAtHght(h4);
                if (ok(pe4)) {
                    const double te4 = prof.interpVtmp(pe4);
                    const double tp4 = wetLift(pe3, tp3, pe4);
                    const double d3 = (virtualTemp(pe3, tp3, tp3) - te3) / ctok(te3);
                    const double d4 = (virtualTemp(pe4, tp4, tp4) - te4) / ctok(te4);
                    const double lyrf = G * (d3 + d4) / 2.0 * (h4 - h3);
                    if (lyrf > 0.0) {
                        b3 += lyrf;
                    }
                }
                pcl.cape3km = b3;
                b3Set = true;
            }
        } else {
            pcl.cape3km = 0.0;
        }
        (void) lcl6;
        (void) b6Set;
        h1 = h2;

        // ---- LFC: energy turns from negative to positive ----
        if (lyre >= 0.0 && lyrlast <= 0.0) {
            const double tp3 = tp1;
            const double pe2x = pe1;
            double pe3 = pelast;
            if (buoyant(pe3, pe2x, tp3, prof.interpVtmp(pe3))) {
                lfcP = pe3;
                elP = missing;
                mplP = missing;
                cinhAtLfc = totn;
            } else {
                while (!buoyant(pe3, pe2x, tp3, prof.interpVtmp(pe3)) && pe3 > 0.0) {
                    pe3 -= 5.0;
                }
                if (pe3 > 0.0) {
                    lfcP = pe3;
                    elP = missing;
                    mplP = missing;
                    cinhAtLfc = totn;
                }
            }
            if (ok(lfcP) && lfcP >= pcl.lclPres) {
                lfcP = pcl.lclPres;
            }
        }
        // ---- EL: energy turns from positive to negative ----
        if (lyre <= 0.0 && lyrlast >= 0.0) {
            const double tp3 = tp1;
            const double pe2x = pe1;
            double pe3 = pelast;
            while (buoyant(pe3, pe2x, tp3, prof.interpVtmp(pe3)) && pe3 > 0.0) {
                pe3 -= 5.0;
            }
            elP = pe3;
            mplP = missing;
        }
        // ---- MPL: past the EL, the level where the negative energy has used up the positive (SHARPpy's loop, h3 not advanced) ----
        if (tote < 0.0 && !ok(mplP) && ok(elP)) {
            double pe3 = pelast;
            const double h3 = prof.interpHght(pe3);
            double te3 = prof.interpVtmp(pe3);
            double tp3 = wetLift(pe1, tp1, pe3);
            double totx = tote - lyre;
            double pe2m = pelast;
            while (totx > 0.0 && pe2m > 1.0) {
                pe2m -= 1.0;
                const double te2m = prof.interpVtmp(pe2m);
                const double tp2m = wetLift(pe3, tp3, pe2m);
                const double h2m = prof.interpHght(pe2m);
                const double tdef3 = (virtualTemp(pe3, tp3, tp3) - te3) / ctok(te3);
                const double tdef2m = (virtualTemp(pe2m, tp2m, tp2m) - te2m) / ctok(te2m);
                totx += G * (tdef3 + tdef2m) / 2.0 * (h2m - h3);
                tp3 = tp2m;
                te3 = te2m;
                pe3 = pe2m;
            }
            mplP = pe2m;
        }
        if (prof.pres[i] <= 500.0 && !liDone5) {
            const double a = prof.interpVtmp(500.0);
            const double b = wetLift(pe1, tp1, 500.0);
            pcl.liftedIndex500 = a - virtualTemp(500.0, b, b);
            liDone5 = true;
        }
        if (prof.pres[i] <= 300.0 && !liDone3) {
            const double a = prof.interpVtmp(300.0);
            const double b = wetLift(pe1, tp1, 300.0);
            pcl.liftedIndex300 = a - virtualTemp(300.0, b, b);
            liDone3 = true;
        }
    }
    (void) pTopProfile;
    (void) tote;
    pcl.cape = totp;
    pcl.cin = cinhAtLfc;
    // no CAPE at all -> no CIN. (SHARPpy zeroes CIN whenever CAPE rounds down to 0; SPC keeps it unless CAPE is exactly 0.)
    if (pcl.cape == 0.0) {
        pcl.cin = 0.0;
    }
    if (ok(lfcP)) {
        pcl.lfcPres = lfcP;
        const double h = prof.interpHght(lfcP);
        pcl.lfcHght = ok(h) ? prof.toAgl(h) : missing;
    }
    if (ok(elP)) {
        pcl.elPres = elP;
        const double h = prof.interpHght(elP);
        pcl.elHght = ok(h) ? prof.toAgl(h) : missing;
    }
    if (ok(mplP)) {
        pcl.mplPres = mplP;
        const double h = prof.interpHght(mplP);
        pcl.mplHght = ok(h) ? prof.toAgl(h) : missing;
    }
    pcl.valid = true;
    return pcl;
}

Parcel lift(const SoundingProfile& prof, Kind kind) {
    if (prof.size() < 3) {
        return {};
    }
    const double sfcP = prof.sfcPres();
    double pres = sfcP;
    double temp = prof.tmpc[static_cast<size_t>(prof.sfc)];
    double dwpt = prof.dwpc[static_cast<size_t>(prof.sfc)];
    if (kind == Kind::MixedLayer) {
        mixedLayerStart(prof, 100.0, pres, temp, dwpt);
    } else if (kind == Kind::MostUnstable) {
        // SHARPpy's most_unstable_level: on a 1 mb grid, lift every parcel dry to its LCL and
        // then moist-adiabatically to 1000 mb; the warmest result marks the most unstable level.
        double best = -1e9;
        for (double pp = sfcP; pp >= sfcP - 400.0 - 1e-9; pp -= 1.0) {
            const double t = prof.interpTemp(pp), d = prof.interpDwpt(pp);
            if (isMissing(t) || isMissing(d)) {
                continue;
            }
            double lp = 0.0, lt = 0.0;
            SoundingThermo::dryLift(pp, t, d, lp, lt);
            const double mt = SoundingThermo::wetLift(lp, lt, 1000.0);
            if (mt > best + 1e-10) {
                best = mt;
                pres = pp;
                temp = t;
                dwpt = d;
            }
        }
    }
    return liftFrom(prof, pres, temp, dwpt);
}

}  // namespace SoundingParcel
