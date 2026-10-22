// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/PhaseSpace.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double earth = 6371.0;
    constexpr double radius = 500.0;   // km

    // the point `km` from lat, lon toward `bearing`
    void destination(double lat, double lon, double bearing, double km, double& outLat, double& outLon) {
        const double d = km / earth, b = bearing * pi / 180.0, la = lat * pi / 180.0, lo = lon * pi / 180.0;
        const double la2 = std::asin(std::sin(la) * std::cos(d) + std::cos(la) * std::sin(d) * std::cos(b));
        const double lo2 = lo + std::atan2(std::sin(b) * std::sin(d) * std::cos(la), std::cos(d) - std::sin(la) * std::sin(la2));
        outLat = la2 * 180.0 / pi;
        outLon = std::fmod(lo2 * 180.0 / pi + 540.0, 360.0) - 180.0;
    }

    // slope of y against ln p by least squares
    bool slope(const std::vector<std::pair<double, double>>& pts, double& out) {
        if (pts.size() < 3) {
            return false;
        }
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (const auto& [p, y] : pts) {
            const double x = std::log(p);
            sx += x; sy += y; sxx += x * x; sxy += x * y;
        }
        const double n = static_cast<double>(pts.size());
        const double den = n * sxx - sx * sx;
        if (std::fabs(den) < 1e-12) {
            return false;
        }
        out = (n * sxy - sx * sy) / den;
        return true;
    }
}

double PhaseSpace::bearing(double lat1, double lon1, double lat2, double lon2) {
    const double la1 = lat1 * pi / 180.0, la2 = lat2 * pi / 180.0, dl = (lon2 - lon1) * pi / 180.0;
    const double y = std::sin(dl) * std::cos(la2), x = std::cos(la1) * std::sin(la2) - std::sin(la1) * std::cos(la2) * std::cos(dl);
    return std::fmod(std::atan2(y, x) * 180.0 / pi + 360.0, 360.0);
}

double PhaseSpace::kilometres(double lat1, double lon1, double lat2, double lon2) {
    const double la1 = lat1 * pi / 180.0, la2 = lat2 * pi / 180.0, dl = (lon2 - lon1) * pi / 180.0;
    const double a = std::sin((la2 - la1) / 2) * std::sin((la2 - la1) / 2) + std::cos(la1) * std::cos(la2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * earth * std::asin(std::min(1.0, std::sqrt(a)));
}

PhaseSpace::Point PhaseSpace::measure(const Levels& heights, double lat, double lon, double heading) {
    Point point;
    point.lat = lat;
    point.lon = lon;
    point.heading = heading;
    // the samples: rings every 100 km (the centre once), every 15 degrees; weighted by the ring's radius (its area)
    struct Sample { double lat, lon, weight, side; };   // side +1 right of the motion, -1 left
    std::vector<Sample> samples;
    samples.push_back({lat, lon, 0.25, 0.0});
    for (double r = 100.0; r <= radius + 1.0; r += 100.0) {
        for (int az = 0; az < 360; az += 15) {
            Sample s{0, 0, r, 0.0};
            destination(lat, lon, az, r, s.lat, s.lon);
            if (heading >= 0.0) {
                const double rel = std::fmod(az - heading + 720.0, 360.0);
                s.side = rel < 180.0 ? 1.0 : -1.0;
            }
            samples.push_back(s);
        }
    }
    std::map<int, double> range;   // the height range at each level
    for (const auto& [level, grid] : heights) {
        double lo = 1e30, hi = -1e30;
        bool bad = false;
        for (const auto& s : samples) {
            const double z = grid.sample(s.lon, s.lat);
            if (std::isnan(z)) {
                bad = true;
                break;
            }
            lo = std::min(lo, z);
            hi = std::max(hi, z);
        }
        if (!bad) {
            range[level] = hi - lo;
        }
    }
    std::vector<std::pair<double, double>> lower, upper;
    for (const int level : {900, 850, 800, 700, 600}) {   // 850 stands in where 900 / 800 are not there; a layer needs three of its levels
        if (range.count(level)) {
            lower.push_back({static_cast<double>(level), range[level]});
        }
    }
    if (range.count(900) && range.count(850) && range.count(800)) {   // all the usual levels: not both 900 and 850
        lower.erase(std::remove_if(lower.begin(), lower.end(), [] (const auto& p) { return p.first == 850.0; }), lower.end());
    }
    for (const int level : {600, 500, 400, 300}) {
        if (range.count(level)) {
            upper.push_back({static_cast<double>(level), range[level]});
        }
    }
    double vtl = 0.0, vtu = 0.0;
    if (!slope(lower, vtl) || !slope(upper, vtu)) {
        return point;
    }
    point.vtl = vtl;
    point.vtu = vtu;
    // B: the thickness 600 - 900 mb, right of the motion minus left, in the northern hemisphere (the other way south)
    if (heading >= 0.0 && heights.count(600) && heights.count(900)) {
        double right = 0, left = 0, wr = 0, wl = 0;
        for (const auto& s : samples) {
            if (s.side == 0.0) {
                continue;
            }
            const double t = heights.at(600).sample(s.lon, s.lat) - heights.at(900).sample(s.lon, s.lat);
            if (std::isnan(t)) {
                return point;
            }
            (s.side > 0 ? right : left) += t * s.weight;
            (s.side > 0 ? wr : wl) += s.weight;
        }
        if (wr > 0 && wl > 0) {
            point.b = (lat >= 0.0 ? 1.0 : -1.0) * (right / wr - left / wl);
        }
    }
    point.bKnown = heading >= 0.0;
    point.ok = true;
    point.state = stateOf(point);
    return point;
}

std::string PhaseSpace::stateOf(const Point& p) {
    const bool asymmetric = p.b > 10.0;
    const bool warm = p.vtl > 0.0;
    return std::string{asymmetric ? "Asymmetric" : "Symmetric"} + (warm ? " warm core" : " cold core");
}

std::string PhaseSpace::wordFor(const Point& p) {
    if (!p.ok) {
        return "Not measurable";
    }
    const bool asymmetric = p.b > 10.0, lowerWarm = p.vtl > 0.0, upperWarm = p.vtu > 0.0;
    if (!asymmetric && lowerWarm && upperWarm) {
        return "Tropical";
    }
    if (!asymmetric && lowerWarm) {
        return "Shallow warm core";
    }
    if (!asymmetric) {
        return "Cold core";
    }
    if (lowerWarm && upperWarm) {
        return "Warm seclusion";
    }
    if (lowerWarm) {
        return "Hybrid";
    }
    return "Extratropical";
}

bool PhaseSpace::lowestNear(const GfsGrid::Grid& pressure, double lat, double lon, double radiusDegrees, double& foundLat, double& foundLon, double& value) {
    if (pressure.empty()) {
        return false;
    }
    double best = 1e30;
    bool any = false;
    const double step = std::max(0.1, pressure.step);
    for (double la = lat - radiusDegrees; la <= lat + radiusDegrees; la += step) {
        for (double lo = lon - radiusDegrees; lo <= lon + radiusDegrees; lo += step) {
            const double v = pressure.sample(lo > 180.0 ? lo - 360.0 : lo < -180.0 ? lo + 360.0 : lo, la);
            if (std::isnan(v)) {
                continue;
            }
            // a mild pull toward where the storm was, so a deeper low far away does not take the track
            const double score = v + (v > 2000.0 ? 30.0 : 0.3) * std::hypot(la - lat, lo - lon);
            if (score < best) {
                best = score;
                value = v;
                foundLat = la;
                foundLon = lo;
                any = true;
            }
        }
    }
    return any;
}

std::vector<PhaseSpace::Point> PhaseSpace::smoothed(const std::vector<Point>& points, long seconds) {
    auto out = points;
    for (size_t i = 0; i < points.size(); i++) {
        if (!points[i].ok) {
            continue;
        }
        double b = 0, vl = 0, vu = 0, hp = 0;
        int n = 0, nb = 0, np = 0;
        for (const auto& q : points) {
            if (q.ok && std::labs(q.valid - points[i].valid) <= seconds) {
                vl += q.vtl;
                vu += q.vtu;
                n++;
                if (q.bKnown) {
                    b += q.b;
                    nb++;
                }
                if (q.pressure > 0) {
                    hp += q.pressure;
                    np++;
                }
            }
        }
        out[i].vtl = vl / n;
        out[i].vtu = vu / n;
        out[i].b = nb ? b / nb : points[i].b;
        out[i].pressure = np ? hp / np : points[i].pressure;
        out[i].state = stateOf(out[i]);
    }
    return out;
}
