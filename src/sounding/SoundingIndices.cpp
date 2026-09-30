// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingIndices.h"
#include <cmath>
#include "sounding/SoundingThermo.h"

namespace SoundingIndices {
    namespace {
        constexpr double missing = -9999.0;
        constexpr double knotsToMs = 0.514444;
        bool gone(double v) { return v <= -9998.0; }
        double ctokelvin(double c) { return c + 273.15; }

        // mean wind over 1 mb steps, optionally weighted by pressure
        Wind mean(const SoundingProfile& p, double fromAgl, double toAgl, bool pressureWeighted) {
            const double pb = presAtAgl(p, fromAgl);
            const double pt = presAtAgl(p, toAgl);
            if (gone(pb) || gone(pt)) return {};
            double su = 0, sv = 0, sw = 0;
            for (double pr = pb; pr >= pt - 1e-9; pr -= 1.0) {
                double u, v;
                if (!p.interpComponents(pr, u, v)) continue;
                const double w = pressureWeighted ? pr : 1.0;
                su += u * w;
                sv += v * w;
                sw += w;
            }
            if (sw <= 0) return {};
            return {su / sw, sv / sw};
        }
    }

    double Wind::speed() const { return valid() ? std::hypot(u, v) : missing; }

    double Wind::direction() const {
        if (!valid()) return missing;
        double d = std::atan2(-u, -v) * 180.0 / M_PI;
        if (d < 0) d += 360.0;
        return d;
    }

    double presAtAgl(const SoundingProfile& p, double hAgl) { return p.interpPresAtHght(p.toMsl(hAgl)); }

    Wind windAtAgl(const SoundingProfile& p, double hAgl) {
        Wind w;
        const double pr = hAgl <= 0 ? p.sfcPres() : presAtAgl(p, hAgl);
        if (gone(pr) || !p.interpComponents(pr, w.u, w.v)) return {};
        return w;
    }

    Wind bulkShear(const SoundingProfile& p, double fromAgl, double toAgl) {
        const Wind a = windAtAgl(p, fromAgl), b = windAtAgl(p, toAgl);
        if (!a.valid() || !b.valid()) return {};
        return {b.u - a.u, b.v - a.v};
    }

    Wind meanWind(const SoundingProfile& p, double fromAgl, double toAgl) { return mean(p, fromAgl, toAgl, true); }

    namespace {
        // Bunkers ID method as SHARPpy's non_parcel_bunkers_motion: the mean 0-6 km wind
        // (not pressure weighted), moved 7.5 m/s perpendicular to the 0-6 km shear vector.
        Wind bunkers(const SoundingProfile& p, double side) {
            const double d = 7.5 / knotsToMs;
            const Wind m6 = mean(p, 0, 6000, false);
            const Wind shear = bulkShear(p, 0, 6000);
            if (!m6.valid() || !shear.valid()) return {};
            const double mag = std::hypot(shear.u, shear.v);
            if (mag < 1e-6) return {};
            return {m6.u + side * d / mag * shear.v, m6.v - side * d / mag * shear.u};
        }
    }

    Wind bunkersRight(const SoundingProfile& p) { return bunkers(p, 1.0); }
    Wind bunkersLeft(const SoundingProfile& p) { return bunkers(p, -1.0); }

    double helicity(const SoundingProfile& p, double fromAgl, double toAgl, const Wind& storm) {
        if (!storm.valid()) return missing;
        const double pb = fromAgl <= 0 ? p.sfcPres() : presAtAgl(p, fromAgl);
        const double pt = presAtAgl(p, toAgl);
        if (gone(pb) || gone(pt)) return missing;
        std::vector<double> us, vs;
        double u, v;
        if (!p.interpComponents(pb, u, v)) return missing;
        us.push_back(u);
        vs.push_back(v);
        for (size_t i = 0; i < p.size(); i += 1) {
            if (p.pres[i] < pb && p.pres[i] > pt && !gone(p.u[i]) && !gone(p.v[i])) {
                us.push_back(p.u[i]);
                vs.push_back(p.v[i]);
            }
        }
        if (!p.interpComponents(pt, u, v)) return missing;
        us.push_back(u);
        vs.push_back(v);
        double sum = 0;
        for (size_t i = 0; i + 1 < us.size(); i += 1) {
            const double u1 = us[i] - storm.u, v1 = vs[i] - storm.v;
            const double u2 = us[i + 1] - storm.u, v2 = vs[i + 1] - storm.v;
            sum += u2 * v1 - u1 * v2;
        }
        return sum * knotsToMs * knotsToMs;
    }

    double mixingRatioAt(const SoundingProfile& p, double pMb) {
        const double td = p.interpDwpt(pMb);
        return gone(td) ? missing : SoundingThermo::mixingRatio(pMb, td);
    }

    double precipitableWater(const SoundingProfile& p, double topMb) {
        const double pb = p.sfcPres();
        if (gone(pb) || topMb >= pb) return missing;
        std::vector<double> ps{pb}, ws{mixingRatioAt(p, pb)};
        for (size_t i = 0; i < p.size(); i += 1) {
            if (p.pres[i] < pb && p.pres[i] > topMb && !gone(p.dwpc[i])) {
                ps.push_back(p.pres[i]);
                ws.push_back(SoundingThermo::mixingRatio(p.pres[i], p.dwpc[i]));
            }
        }
        ps.push_back(topMb);
        ws.push_back(mixingRatioAt(p, topMb));
        double pw = 0;
        for (size_t i = 0; i + 1 < ps.size(); i += 1) {
            if (gone(ws[i]) || gone(ws[i + 1])) return missing;
            pw += 0.5 * (ws[i] + ws[i + 1]) * (ps[i] - ps[i + 1]);
        }
        return pw * 0.00040173;   // g/kg * mb -> inches of liquid water
    }

    double meanMixingRatio(const SoundingProfile& p, double fromAgl, double toAgl) {
        const double pb = fromAgl <= 0 ? p.sfcPres() : presAtAgl(p, fromAgl);
        const double pt = presAtAgl(p, toAgl);
        if (gone(pb) || gone(pt)) return missing;
        double s = 0, sw = 0;
        for (double pr = pb; pr >= pt - 1e-9; pr -= 1.0) {
            const double w = mixingRatioAt(p, pr);
            if (gone(w)) continue;
            s += w * pr;
            sw += pr;
        }
        return sw > 0 ? s / sw : missing;
    }

    double deltaT(const SoundingProfile& p, double fromMb, double toMb) {
        const double a = p.interpTemp(fromMb), b = p.interpTemp(toMb);
        return gone(a) || gone(b) ? missing : b - a;
    }

    double lapseRateMb(const SoundingProfile& p, double fromMb, double toMb) {
        const double a = p.interpTemp(fromMb), b = p.interpTemp(toMb);
        const double ha = p.interpHght(fromMb), hb = p.interpHght(toMb);
        if (gone(a) || gone(b) || gone(ha) || gone(hb) || hb == ha) return missing;
        return (a - b) / (hb - ha) * 1000.0;
    }

    double lapseRateAgl(const SoundingProfile& p, double fromAgl, double toAgl) {
        const double pa = fromAgl <= 0 ? p.sfcPres() : presAtAgl(p, fromAgl);
        const double pb = presAtAgl(p, toAgl);
        if (gone(pa) || gone(pb)) return missing;
        const double a = p.interpTemp(pa), b = p.interpTemp(pb);
        if (gone(a) || gone(b)) return missing;
        return (a - b) / (toAgl - fromAgl) * 1000.0;
    }

    double surfaceRelativeHumidity(const SoundingProfile& p) {
        const size_t s = static_cast<size_t>(p.sfc);
        return SoundingThermo::relativeHumidityPct(p.pres[s], p.tmpc[s], p.dwpc[s]);
    }
}

// ---- Ported from SHARPpy (params.py / winds.py): Copyright (c) 2011 Patrick T. Marsh & John Hart,
// 2012 MetPy Developers, 2020 Kelton Halbert, Greg Blumberg & Tim Supinie; BSD-3-Clause. See
// docs/sharppy-notice.md. ----
namespace SoundingIndices {
    EffectiveLayer effectiveInflowLayer(const SoundingProfile& p, const SoundingParcel::Parcel& mu, double minCape, double maxCin) {
        EffectiveLayer out;
        if (mu.cape == 0.0 || mu.cape < minCape || !(mu.cin > maxCin)) return out;
        size_t last = 0;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (!gone(p.tmpc[i])) last = i;
        }
        auto lifted = [&](size_t i) { return SoundingParcel::liftFrom(p, p.pres[i], p.tmpc[i], p.dwpc[i]); };
        size_t bottom = p.size();
        for (size_t i = static_cast<size_t>(p.sfc); i < last; i += 1) {
            if (gone(p.tmpc[i]) || gone(p.dwpc[i])) continue;
            const auto pcl = lifted(i);
            if (pcl.valid && pcl.cape >= minCape && pcl.cin > maxCin) {
                bottom = i;
                break;
            }
        }
        if (bottom >= p.size()) return out;
        double pTop = missing;
        size_t prev = bottom;
        for (size_t i = bottom + 1; i < last; i += 1) {
            // SHARPpy skips levels whose dewpoint or temperature is missing (or exactly zero)
            if (gone(p.dwpc[i]) || gone(p.tmpc[i]) || p.dwpc[i] == 0.0 || p.tmpc[i] == 0.0) continue;
            const auto pcl = lifted(i);
            if (!pcl.valid || pcl.cape < minCape || pcl.cin <= maxCin) {
                pTop = std::fmin(p.pres[prev], p.pres[bottom]);
                break;
            }
            prev = i;
        }
        if (gone(pTop)) return out;
        out.valid = true;
        out.pBot = p.pres[bottom];
        out.pTop = pTop;
        out.botAgl = p.toAgl(p.interpHght(out.pBot));
        out.topAgl = p.toAgl(p.interpHght(out.pTop));
        return out;
    }

    Wind stormMotion(const SoundingProfile& p, const SoundingParcel::Parcel& mu, const EffectiveLayer& layer, bool leftMover) {
        if (!layer.valid || !(mu.cape > 100.0) || gone(mu.elHght)) return bunkers(p, leftMover ? -1.0 : 1.0);
        const double d = 7.5 / knotsToMs;
        const double depth = mu.elHght - layer.botAgl;
        const double pTop = presAtAgl(p, layer.botAgl + depth * 0.65);
        if (gone(pTop)) return {};
        // pressure-weighted mean wind and the shear vector over the layer, in pressure coordinates
        double su = 0, sv = 0, sw = 0;
        for (double pr = layer.pBot; pr >= pTop - 1e-9; pr -= 1.0) {
            double u, v;
            if (!p.interpComponents(pr, u, v)) continue;
            su += u * pr;
            sv += v * pr;
            sw += pr;
        }
        double ub, vb, ut, vt;
        if (sw <= 0 || !p.interpComponents(layer.pBot, ub, vb) || !p.interpComponents(pTop, ut, vt)) return {};
        const double shu = ut - ub, shv = vt - vb;
        const double mag = std::hypot(shu, shv);
        if (mag < 1e-6) return {};
        const double side = leftMover ? -1.0 : 1.0;
        return {su / sw + side * d / mag * shv, sv / sw - side * d / mag * shu};
    }

    double effectiveBulkShear(const SoundingProfile& p, const SoundingParcel::Parcel& mu, const EffectiveLayer& layer) {
        if (!layer.valid || gone(mu.elHght)) return missing;
        const double depth = (mu.elHght - layer.botAgl) / 2.0;
        const double pTop = presAtAgl(p, layer.botAgl + depth);
        double ub, vb, ut, vt;
        if (gone(pTop) || !p.interpComponents(layer.pBot, ub, vb) || !p.interpComponents(pTop, ut, vt)) return missing;
        return std::hypot(ut - ub, vt - vb);
    }

    double temperatureLevel(const SoundingProfile& p, double tempC, bool wetBulbProfile) {
        const auto& series = wetBulbProfile ? p.wetbulb : p.tmpc;
        std::vector<double> t, lp;
        bool below = false, above = false;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (gone(series[i]) || gone(p.pres[i]) || p.pres[i] <= 0) continue;
            if (series[i] == tempC) return p.pres[i];
            below = below || series[i] < tempC;
            above = above || series[i] > tempC;
            t.push_back(series[i]);
            lp.push_back(std::log10(p.pres[i]));
        }
        if (!below || !above) return missing;
        for (size_t i = 0; i + 1 < t.size(); i += 1) {
            if ((t[i] - tempC) * (t[i + 1] - tempC) < 0) {
                const double f = (tempC - t[i]) / (t[i + 1] - t[i]);
                return std::pow(10.0, lp[i] + f * (lp[i + 1] - lp[i]));
            }
        }
        return missing;
    }

    double dcape(const SoundingProfile& p) {
        const double sfcP = p.sfcPres();
        // the downdraft source: centre of the driest (lowest mean theta-e) 100 mb layer in the lowest 400 mb
        double minMean = 1000.0, minP = missing;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (gone(p.thetae[i]) || p.pres[i] < sfcP - 400.0) continue;
            double s = 0, sw = 0;
            bool okLayer = true;
            for (double pr = p.pres[i]; pr >= p.pres[i] - 100.0 - 1e-9; pr -= 1.0) {
                const double te = p.interpThetae(pr);
                if (gone(te)) { okLayer = false; break; }
                s += te * pr;
                sw += pr;
            }
            if (okLayer && sw > 0 && s / sw < minMean) {
                minMean = s / sw;
                minP = p.pres[i] - 50.0;
            }
        }
        if (gone(minP)) return missing;
        // levels (with valid theta-e) at or above the source, walked back down to the surface
        std::vector<size_t> idx;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (!gone(p.thetae[i]) && !gone(p.pres[i])) idx.push_back(i);
        }
        size_t top = 0;
        bool found = false;
        for (size_t k = 0; k < idx.size(); k += 1) {
            if (p.pres[idx[k]] >= minP) { top = k; found = true; }
        }
        if (!found) return missing;
        double pe1 = minP, te1 = p.interpTemp(pe1), h1 = p.interpHght(pe1);
        double tp1 = SoundingThermo::wetBulb(minP, te1, p.interpDwpt(minP));
        double tote = 0, lyre = 0;
        for (size_t k = top + 1; k-- > 0;) {
            const size_t i = idx[k];
            const double pe2 = p.pres[i], te2 = p.tmpc[i], h2 = p.hght[i];
            const double tp2 = SoundingThermo::wetLift(pe1, tp1, pe2);
            if (!gone(te1) && !gone(te2)) {
                const double d1 = (tp1 - te1) / ctokelvin(te1);
                const double d2 = (tp2 - te2) / ctokelvin(te2);
                lyre = 9.8 * (d1 + d2) / 2.0 * (h2 - h1);
                tote += lyre;
            }
            pe1 = pe2;
            te1 = te2;
            h1 = h2;
            tp1 = tp2;
        }
        return tote;
    }

    double stpFixed(double sbCape, double sbLclM, double srh01, double bwd6Ms) {
        const double lclTerm = sbLclM < 1000.0 ? 1.0 : sbLclM > 2000.0 ? 0.0 : (2000.0 - sbLclM) / 1000.0;
        if (bwd6Ms > 30.0) bwd6Ms = 30.0;
        else if (bwd6Ms < 12.5) bwd6Ms = 0.0;
        return sbCape / 1500.0 * lclTerm * (srh01 / 150.0) * (bwd6Ms / 20.0);
    }

    double stpEffective(double mlCape, double effSrh, double effBwdMs, double mlLclM, double mlCin) {
        const double bwdTerm = effBwdMs < 12.5 ? 0.0 : effBwdMs > 30.0 ? 1.5 : effBwdMs / 20.0;
        const double lclTerm = mlLclM < 1000.0 ? 1.0 : mlLclM > 2000.0 ? 0.0 : (2000.0 - mlLclM) / 1000.0;
        const double cinTerm = mlCin > -50.0 ? 1.0 : mlCin < -200.0 ? 0.0 : (mlCin + 200.0) / 150.0;
        return std::fmax(mlCape / 1500.0 * (effSrh / 150.0) * bwdTerm * lclTerm * cinTerm, 0.0);
    }

    double supercellComposite(double muCape, double effSrh, double effBwdMs) {
        if (effBwdMs > 20.0) effBwdMs = 20.0;
        else if (effBwdMs < 10.0) effBwdMs = 0.0;
        return muCape / 1000.0 * (effSrh / 50.0) * (effBwdMs / 20.0);
    }

    double significantHail(const SoundingProfile& p, const SoundingParcel::Parcel& mu) {
        if (gone(mu.lplPres) || gone(mu.lplDwpt)) return missing;
        double mumr = SoundingThermo::mixingRatio(mu.lplPres, mu.lplDwpt);
        const double frzP = temperatureLevel(p, 0.0);
        const double frzH = gone(frzP) ? missing : p.interpHght(frzP);
        double h5 = p.interpTemp(500.0);
        const double lr75 = lapseRateMb(p, 700.0, 500.0);
        const Wind shear = bulkShear(p, 0, 6000);
        if (gone(frzH) || gone(h5) || gone(lr75) || !shear.valid()) return missing;
        double shr06 = shear.speed() * knotsToMs;
        shr06 = std::fmin(27.0, std::fmax(7.0, shr06));
        mumr = std::fmin(13.6, std::fmax(11.0, mumr));
        h5 = std::fmin(-5.5, h5);
        double ship = -1.0 * (mu.cape * mumr * lr75 * h5 * shr06) / 42000000.0;
        if (mu.cape < 1300.0) ship *= mu.cape / 1300.0;
        if (lr75 < 5.8) ship *= lr75 / 5.8;
        if (frzH < 2400.0) ship *= frzH / 2400.0;
        return ship;
    }
}
