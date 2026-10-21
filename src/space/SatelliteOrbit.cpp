// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SatelliteOrbit.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace SatelliteOrbit {
    namespace {
        constexpr double pi = std::numbers::pi;
        constexpr double twoPi = 2.0 * std::numbers::pi;
        constexpr double deg = pi / 180.0;
        // the constants of the WGS 72 model the elements are made for
        constexpr double ke = 0.0743669161331734132;   // sqrt(GM) in earth radii ^ 1.5 per minute
        constexpr double earthRadiusKm = 6378.135;
        constexpr double j2 = 0.001082616, j3 = -0.00000253881, j4 = -0.00000165597;
        constexpr double ck2 = 0.5 * j2, ck4 = -0.375 * j4;
        constexpr double meanRadiusKm = 6371.0;

        double wrap(double x) {
            x = std::fmod(x, twoPi);
            return x < 0.0 ? x + twoPi : x;
        }

        // the epoch of a line: the year (two digits) and the day of the year with its fraction
        double epochOf(int yy, double day) {
            const int year = yy < 57 ? 2000 + yy : 1900 + yy;
            const long a = (14 - 1) / 12;
            const long y = year + 4800 - a;
            const long m = 1 + 12 * a - 3;
            const double jd0 = 1 + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045 - 0.5;   // 1 January 0h
            return jd0 + day - 1.0;
        }

        // the "assumed decimal point" fields of a line ("-11606-4" is -0.11606e-4)
        double implied(const string& field) {
            string f = field;
            while (!f.empty() && f.front() == ' ') f.erase(f.begin());
            if (f.size() < 3) return 0.0;
            const auto sign = f.front() == '-' ? -1.0 : 1.0;
            if (f.front() == '-' || f.front() == '+') f.erase(f.begin());
            const auto split = f.find_last_of("+-");
            if (split == string::npos || split == 0) return 0.0;
            return sign * std::atof(("0." + f.substr(0, split)).c_str()) * std::pow(10.0, std::atoi(f.substr(split).c_str()));
        }

        double gmst(double jd) {   // radians
            const double d = jd - 2451545.0, t = d / 36525.0;
            return wrap((280.46061837 + 360.98564736629 * d + 0.000387933 * t * t - t * t * t / 38710000.0) * deg);
        }

        struct Vec {
            double x, y, z;
        };
        Vec cross(const Vec& a, const Vec& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
        double dot(const Vec& a, const Vec& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        Vec unit(const Vec& a) {
            const double n = std::sqrt(dot(a, a));
            return n > 0.0 ? Vec{a.x / n, a.y / n, a.z / n} : a;
        }

        // the earth fixed unit vector under the satellite (geocentric)
        Vec ecefUnit(const State& s, double jd) {
            const double g = gmst(jd), c = std::cos(g), sn = std::sin(g);
            return unit({s.x * c + s.y * sn, -s.x * sn + s.y * c, s.z});
        }
        Vec fromLatLon(double lat, double lon) {
            return {std::cos(lat * deg) * std::cos(lon * deg), std::cos(lat * deg) * std::sin(lon * deg), std::sin(lat * deg)};
        }
        void toLatLon(const Vec& v, double& lat, double& lon) {
            lat = std::atan2(v.z, std::hypot(v.x, v.y)) / deg;
            lon = std::atan2(v.y, v.x) / deg;
        }
    }

    double julianDate(long long unixSeconds) {
        return static_cast<double>(unixSeconds) / 86400.0 + 2440587.5;
    }
    long long unixSeconds(double jd) {
        return static_cast<long long>(std::llround((jd - 2440587.5) * 86400.0));
    }

    Tle parse(const string& name, const string& l1, const string& l2) {
        Tle t;
        if (l1.size() < 63 || l2.size() < 63 || l1[0] != '1' || l2[0] != '2') {
            return t;
        }
        t.name = name;
        while (!t.name.empty() && (t.name.back() == ' ' || t.name.back() == '\r')) t.name.pop_back();
        if (t.name.compare(0, 2, "0 ") == 0) t.name.erase(0, 2);
        t.catalog = std::atoi(l1.substr(2, 5).c_str());
        t.epochJd = epochOf(std::atoi(l1.substr(18, 2).c_str()), std::atof(l1.substr(20, 12).c_str()));
        t.bstar = implied(l1.substr(53, 8));
        t.inclination = std::atof(l2.substr(8, 8).c_str()) * deg;
        t.node = std::atof(l2.substr(17, 8).c_str()) * deg;
        t.eccentricity = std::atof(("0." + l2.substr(26, 7)).c_str());
        t.argPerigee = std::atof(l2.substr(34, 8).c_str()) * deg;
        t.meanAnomaly = std::atof(l2.substr(43, 8).c_str()) * deg;
        t.meanMotion = std::atof(l2.substr(52, 11).c_str()) * twoPi / 1440.0;
        t.valid = t.meanMotion > 0.0 && t.eccentricity < 1.0;
        return t;
    }

    vector<Tle> parseAll(const string& text) {
        vector<Tle> out;
        vector<string> lines;
        size_t at = 0;
        while (at < text.size()) {
            auto end = text.find('\n', at);
            end = end == string::npos ? text.size() : end;
            string line = text.substr(at, end - at);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            lines.push_back(line);
            at = end + 1;
        }
        for (size_t i = 0; i + 1 < lines.size(); i++) {
            if (lines[i].size() > 1 && lines[i][0] == '1' && lines[i][1] == ' ' && lines[i + 1].size() > 1 && lines[i + 1][0] == '2') {
                const auto t = parse(i > 0 && !(lines[i - 1][0] == '2' && lines[i - 1][1] == ' ') ? lines[i - 1] : string{}, lines[i], lines[i + 1]);
                if (t.valid) {
                    out.push_back(t);
                }
                i++;
            }
        }
        return out;
    }

    Propagator::Propagator(const Tle& tle) : elements{tle} {
        const double no = tle.meanMotion, ecco = tle.eccentricity, inclo = tle.inclination, bstar = tle.bstar, argpo = tle.argPerigee, mo = tle.meanAnomaly;
        period = twoPi / no;
        const double a1 = std::pow(ke / no, 2.0 / 3.0);
        cosio = std::cos(inclo);
        sinio = std::sin(inclo);
        theta2 = cosio * cosio;
        x3thm1 = 3.0 * theta2 - 1.0;
        const double eosq = ecco * ecco, betao2 = 1.0 - eosq, betao = std::sqrt(betao2);
        const double del1 = 1.5 * ck2 * x3thm1 / (a1 * a1 * betao * betao2);
        const double ao = a1 * (1.0 - del1 * (1.0 / 3.0 + del1 * (1.0 + 134.0 / 81.0 * del1)));
        const double delo = 1.5 * ck2 * x3thm1 / (ao * ao * betao * betao2);
        xnodp = no / (1.0 + delo);
        aodp = ao / (1.0 - delo);
        const double perigee = (aodp * (1.0 - ecco) - 1.0) * earthRadiusKm;
        simple = perigee < 220.0;
        double s4 = 1.0 + 78.0 / earthRadiusKm, qoms24 = std::pow(42.0 / earthRadiusKm, 4);
        if (perigee < 156.0) {
            const double sh = perigee < 98.0 ? 20.0 : perigee - 78.0;
            qoms24 = std::pow((120.0 - sh) / earthRadiusKm, 4);
            s4 = sh / earthRadiusKm + 1.0;
        }
        const double pinvsq = 1.0 / (aodp * aodp * betao2 * betao2);
        const double tsi = 1.0 / (aodp - s4);
        eta = aodp * ecco * tsi;
        const double etasq = eta * eta, eeta = ecco * eta, psisq = std::fabs(1.0 - etasq);
        const double coef = qoms24 * std::pow(tsi, 4), coef1 = coef / std::pow(psisq, 3.5);
        const double c2 = coef1 * xnodp * (aodp * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) + 0.75 * ck2 * tsi / psisq * x3thm1 * (8.0 + 3.0 * etasq * (8.0 + etasq)));
        c1 = bstar * c2;
        const double a3ovk2 = -j3 / ck2;
        const double c3 = ecco > 1e-4 ? coef * tsi * a3ovk2 * xnodp * sinio / ecco : 0.0;
        x1mth2 = 1.0 - theta2;
        c4 = 2.0 * xnodp * coef1 * aodp * betao2 * (eta * (2.0 + 0.5 * etasq) + ecco * (0.5 + 2.0 * etasq) -
             2.0 * ck2 * tsi / (aodp * psisq) * (-3.0 * x3thm1 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) + 0.75 * x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) * std::cos(2.0 * argpo)));
        c5 = 2.0 * coef1 * aodp * betao2 * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);
        const double theta4 = theta2 * theta2;
        const double temp1 = 3.0 * ck2 * pinvsq * xnodp, temp2 = temp1 * ck2 * pinvsq, temp3 = 1.25 * ck4 * pinvsq * pinvsq * xnodp;
        xmdot = xnodp + 0.5 * temp1 * betao * x3thm1 + 0.0625 * temp2 * betao * (13.0 - 78.0 * theta2 + 137.0 * theta4);
        const double x1m5th = 1.0 - 5.0 * theta2;
        omgdot = -0.5 * temp1 * x1m5th + 0.0625 * temp2 * (7.0 - 114.0 * theta2 + 395.0 * theta4) + temp3 * (3.0 - 36.0 * theta2 + 49.0 * theta4);
        const double xhdot1 = -temp1 * cosio;
        xnodot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * theta2) + 2.0 * temp3 * (3.0 - 7.0 * theta2)) * cosio;
        omgcof = bstar * c3 * std::cos(argpo);
        xmcof = eeta > 1e-12 ? -2.0 / 3.0 * coef * bstar / eeta : 0.0;
        xnodcf = 3.5 * betao2 * xhdot1 * c1;
        t2cof = 1.5 * c1;
        const double denominator = std::fabs(1.0 + cosio) > 1.5e-12 ? 1.0 + cosio : 1.5e-12;
        xlcof = 0.125 * a3ovk2 * sinio * (3.0 + 5.0 * cosio) / denominator;
        aycof = 0.25 * a3ovk2 * sinio;
        delmo = std::pow(1.0 + eta * std::cos(mo), 3);
        sinmo = std::sin(mo);
        x7thm1 = 7.0 * theta2 - 1.0;
        if (!simple) {
            const double c1sq = c1 * c1;
            d2 = 4.0 * aodp * tsi * c1sq;
            const double temp = d2 * tsi * c1 / 3.0;
            d3 = (17.0 * aodp + s4) * temp;
            d4 = 0.5 * temp * aodp * tsi * (221.0 * aodp + 31.0 * s4) * c1;
            t3cof = d2 + 2.0 * c1sq;
            t4cof = 0.25 * (3.0 * d3 + c1 * (12.0 * d2 + 10.0 * c1sq));
            t5cof = 0.2 * (3.0 * d4 + 12.0 * c1 * d3 + 6.0 * d2 * d2 + 15.0 * c1sq * (2.0 * d2 + c1sq));
        }
    }

    State Propagator::at(double jd) const {
        State out{};
        if (!elements.valid || deepSpace()) {
            return out;
        }
        const double t = (jd - elements.epochJd) * 1440.0;   // minutes since the epoch
        const double ecco = elements.eccentricity, bstar = elements.bstar;
        const double xmdf = elements.meanAnomaly + xmdot * t;
        const double omgadf = elements.argPerigee + omgdot * t;
        const double xnoddf = elements.node + xnodot * t;
        double omega = omgadf, xmp = xmdf;
        const double tsq = t * t;
        const double xnode = xnoddf + xnodcf * tsq;
        double tempa = 1.0 - c1 * t, tempe = bstar * c4 * t, templ = t2cof * tsq;
        if (!simple) {
            const double delomg = omgcof * t;
            const double delm = xmcof * (std::pow(1.0 + eta * std::cos(xmdf), 3) - delmo);
            const double temp = delomg + delm;
            xmp = xmdf + temp;
            omega = omgadf - temp;
            const double tcube = tsq * t, tfour = t * tcube;
            tempa = tempa - d2 * tsq - d3 * tcube - d4 * tfour;
            tempe = tempe + bstar * c5 * (std::sin(xmp) - sinmo);
            templ = templ + t3cof * tcube + tfour * (t4cof + t * t5cof);
        }
        const double a = aodp * tempa * tempa;
        double e = ecco - tempe;
        e = std::max(e, 1e-6);
        const double xl = xmp + omega + xnode + xnodp * templ;
        const double beta = std::sqrt(1.0 - e * e);
        const double xn = ke / std::pow(a, 1.5);
        // the long period periodics
        const double axn = e * std::cos(omega);
        double temp = 1.0 / (a * beta * beta);
        const double xll = temp * xlcof * axn;
        const double aynl = temp * aycof;
        const double xlt = xl + xll;
        const double ayn = e * std::sin(omega) + aynl;
        // Kepler's equation
        const double capu = std::fmod(xlt - xnode, twoPi);
        double epw = capu, sinepw = 0, cosepw = 1, temp3 = 0, temp4 = 0, temp5 = 0, temp6 = 0;
        for (int i = 0; i < 10; i++) {
            sinepw = std::sin(epw);
            cosepw = std::cos(epw);
            temp3 = axn * sinepw;
            temp4 = ayn * cosepw;
            temp5 = axn * cosepw;
            temp6 = ayn * sinepw;
            const double next = (capu - temp4 + temp3 - epw) / (1.0 - temp5 - temp6) + epw;
            if (std::fabs(next - epw) <= 1e-12) {
                epw = next;
                sinepw = std::sin(epw);
                cosepw = std::cos(epw);
                temp3 = axn * sinepw;
                temp4 = ayn * cosepw;
                temp5 = axn * cosepw;
                temp6 = ayn * sinepw;
                break;
            }
            epw = next;
        }
        // the short period terms
        const double ecose = temp5 + temp6, esine = temp3 - temp4;
        const double elsq = axn * axn + ayn * ayn;
        temp = 1.0 - elsq;
        const double pl = a * temp, r = a * (1.0 - ecose);
        const double temp1 = 1.0 / r;
        const double rdot = ke * std::sqrt(a) * esine * temp1;
        const double rfdot = ke * std::sqrt(pl) * temp1;
        const double temp2 = a * temp1;
        const double betal = std::sqrt(temp);
        const double tempr = 1.0 / (1.0 + betal);
        const double cosu = temp2 * (cosepw - axn + ayn * esine * tempr);
        const double sinu = temp2 * (sinepw - ayn - axn * esine * tempr);
        const double u = std::atan2(sinu, cosu);
        const double sin2u = 2.0 * sinu * cosu, cos2u = 2.0 * cosu * cosu - 1.0;
        const double q = 1.0 / pl, p1 = ck2 * q, p2 = p1 * q;
        const double rk = r * (1.0 - 1.5 * p2 * betal * x3thm1) + 0.5 * p1 * x1mth2 * cos2u;
        const double uk = u - 0.25 * p2 * x7thm1 * sin2u;
        const double xnodek = xnode + 1.5 * p2 * cosio * sin2u;
        const double xinck = elements.inclination + 1.5 * p2 * cosio * sinio * cos2u;
        const double rdotk = rdot - xn * p1 * x1mth2 * sin2u;
        const double rfdotk = rfdot + xn * p1 * (x1mth2 * cos2u + 1.5 * x3thm1);
        const double sinuk = std::sin(uk), cosuk = std::cos(uk), sinik = std::sin(xinck), cosik = std::cos(xinck), sinnok = std::sin(xnodek), cosnok = std::cos(xnodek);
        const double xmx = -sinnok * cosik, xmy = cosnok * cosik;
        const double ux = xmx * sinuk + cosnok * cosuk, uy = xmy * sinuk + sinnok * cosuk, uz = sinik * sinuk;
        const double vx = xmx * cosuk - cosnok * sinuk, vy = xmy * cosuk - sinnok * sinuk, vz = sinik * cosuk;
        out.x = rk * ux * earthRadiusKm;
        out.y = rk * uy * earthRadiusKm;
        out.z = rk * uz * earthRadiusKm;
        const double k = earthRadiusKm / 60.0;
        out.vx = (rdotk * ux + rfdotk * vx) * k;
        out.vy = (rdotk * uy + rfdotk * vy) * k;
        out.vz = (rdotk * uz + rfdotk * vz) * k;
        out.ok = rk > 0.0 && std::isfinite(out.x);
        return out;
    }

    Ground groundOf(const State& s, double jd) {
        const double g = gmst(jd), c = std::cos(g), sn = std::sin(g);
        const double x = s.x * c + s.y * sn, y = -s.x * sn + s.y * c, z = s.z;
        constexpr double a = 6378.137, f = 1.0 / 298.257223563, e2 = f * (2.0 - f);
        const double rho = std::hypot(x, y);
        double lat = std::atan2(z, rho * (1.0 - e2));
        double n = a;
        for (int i = 0; i < 5; i++) {
            n = a / std::sqrt(1.0 - e2 * std::sin(lat) * std::sin(lat));
            lat = std::atan2(z + e2 * n * std::sin(lat), rho);
        }
        return {lat / deg, std::atan2(y, x) / deg, rho / std::cos(lat) - n};
    }

    double sunElevation(double lat, double lon, long long when) {
        const double jd = julianDate(when), n = jd - 2451545.0;
        const double L = wrap((280.460 + 0.9856474 * n) * deg), g = wrap((357.528 + 0.9856003 * n) * deg);
        const double lambda = L + (1.915 * std::sin(g) + 0.020 * std::sin(2.0 * g)) * deg;
        const double eps = (23.439 - 0.0000004 * n) * deg;
        const double ra = std::atan2(std::cos(eps) * std::sin(lambda), std::cos(lambda)), dec = std::asin(std::sin(eps) * std::sin(lambda));
        const double h = gmst(jd) + lon * deg - ra;
        return std::asin(std::sin(lat * deg) * std::sin(dec) + std::cos(lat * deg) * std::cos(dec) * std::cos(h)) / deg;
    }

    namespace {
        struct Sample {
            Vec s;       // earth fixed, unit
            Vec ahead;   // the way it goes: the position a moment later minus now
        };
        Sample sampleAt(const Propagator& sat, long long when) {
            const auto a = sat.at(julianDate(when)), b = sat.at(julianDate(when) + 10.0 / 86400.0);
            const Vec s = ecefUnit(a, julianDate(when)), n = ecefUnit(b, julianDate(when) + 10.0 / 86400.0);
            return {s, {n.x - s.x, n.y - s.y, n.z - s.z}};
        }
        // the signed distance (km) of a place from the ground track: positive to the right of the way it goes
        double crossTrack(const Sample& m, const Vec& place) {
            const Vec right = unit(cross(unit(m.ahead), m.s));   // heading x up = right
            return std::asin(std::clamp(dot(place, right), -1.0, 1.0)) * meanRadiusKm;
        }
    }

    vector<Pass> passes(const Propagator& sat, double lat, double lon, long long from, double hours, const Sensor& sensor) {
        vector<Pass> out;
        if (!sat.tle().valid || sat.deepSpace()) {
            return out;
        }
        const Vec place = fromLatLon(lat, lon);
        const long long step = 30, end = from + static_cast<long long>(hours * 3600.0);
        const double near = std::cos((sensor.halfWidthKm + 150.0) / meanRadiusKm);   // within reach of the swath, roughly
        double previous2 = -2.0, previous = -2.0;
        long long time2 = from, time1 = from;
        for (long long t = from; t <= end + step; t += step) {
            const auto a = sat.at(julianDate(t));
            if (!a.ok) {
                continue;
            }
            const double d = dot(place, ecefUnit(a, julianDate(t)));
            if (previous > previous2 && previous >= d && previous > near) {   // a maximum of the cosine: the closest approach, between time2 and t
                long long lo = time2, hi = t;
                while (hi - lo > 3) {   // ternary search on the closest approach, then a look at the last seconds
                    const long long m1 = lo + (hi - lo) / 3, m2 = hi - (hi - lo) / 3;
                    const double d1 = dot(place, ecefUnit(sat.at(julianDate(m1)), julianDate(m1))), d2 = dot(place, ecefUnit(sat.at(julianDate(m2)), julianDate(m2)));
                    if (d1 < d2) lo = m1; else hi = m2;
                }
                long long best = lo;
                double bestDot = -2.0;
                for (long long c = lo; c <= hi; c++) {
                    const double dc = dot(place, ecefUnit(sat.at(julianDate(c)), julianDate(c)));
                    if (dc > bestDot) {
                        bestDot = dc;
                        best = c;
                    }
                }
                if (best >= from && best <= end) {
                    const auto m = sampleAt(sat, best);
                    Pass p;
                    p.time = best;
                    p.distanceKm = crossTrack(m, place);
                    const double reach = std::fabs(p.distanceKm);
                    p.inSwath = reach <= sensor.halfWidthKm && reach >= sensor.innerKm;
                    p.ascending = m.ahead.z > 0.0;
                    toLatLon(m.s, p.satLat, p.satLon);
                    p.sunElevation = sunElevation(lat, lon, best);
                    if (p.inSwath) {
                        out.push_back(p);
                    }
                }
            }
            previous2 = previous;
            previous = d;
            time2 = time1;
            time1 = t;
        }
        return out;
    }

    vector<TrackPoint> track(const Propagator& sat, long long from, double seconds, double stepSeconds) {
        vector<TrackPoint> out;
        for (double s = 0.0; s <= seconds; s += stepSeconds) {
            const long long t = from + static_cast<long long>(s);
            const auto a = sat.at(julianDate(t));
            if (!a.ok) {
                continue;
            }
            const auto g = groundOf(a, julianDate(t));
            out.push_back({t, g.lat, g.lon, a.vz > 0.0});
        }
        return out;
    }

    void swathEdge(const Propagator& sat, long long when, double offsetKm, double& lat, double& lon) {
        const auto m = sampleAt(sat, when);
        const Vec right = unit(cross(unit(m.ahead), m.s));
        const double alpha = offsetKm / meanRadiusKm;
        const Vec p = unit({std::cos(alpha) * m.s.x + std::sin(alpha) * right.x, std::cos(alpha) * m.s.y + std::sin(alpha) * right.y, std::cos(alpha) * m.s.z + std::sin(alpha) * right.z});
        toLatLon(p, lat, lon);
    }
}
