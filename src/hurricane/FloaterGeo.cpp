// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/FloaterGeo.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace FloaterGeo {

std::vector<int> graticule(const unsigned char * g, int w, int h, bool vertical) {
    // a graticule line is a one pixel stripe that differs from the pixels two either side of it on nearly every row (or column) it crosses; the picture's own clouds and coasts do not
    const int along = vertical ? h : w, across = vertical ? w : h;
    // the caption strip at the bottom and the NOAA logo at the bottom left are left out of the count
    const int from = along / 50;
    const int to = vertical ? along - along * 10 / 100 : along - along * 2 / 100;
    const int skipUntil = vertical ? 0 : w / 9;   // the columns of the logo, when scanning rows
    std::vector<double> score(static_cast<size_t>(across), 0.0), strength(static_cast<size_t>(across), 0.0);
    for (int c = 3; c < across - 3; c++) {
        int hits = 0, total = 0;
        double sum = 0.0;
        for (int a = std::max(from, vertical ? 0 : skipUntil); a < to; a += 3) {
            const auto at = [&] (int k) { return static_cast<int>(vertical ? g[a * w + k] : g[k * w + a]); };
            const int v = at(c);
            const int around = (at(c - 2) + at(c + 2)) / 2;
            hits += std::abs(v - around) > 6 ? 1 : 0;
            sum += std::abs(v - around);
            total++;
        }
        score[static_cast<size_t>(c)] = total ? static_cast<double>(hits) / total : 0.0;
        strength[static_cast<size_t>(c)] = total ? sum / total : 0.0;
    }
    std::vector<int> found;
    for (int c = 3; c < across - 3; c++) {
        if (score[static_cast<size_t>(c)] < 0.85 || (!vertical && c >= h * 965 / 1000)) {   // (a row in the caption strip is not a graticule line)
            continue;
        }
        // the two columns either side of a line differ from their own neighbors too (one of those is the line): the line itself differs most
        const double s = strength[static_cast<size_t>(c)];
        bool best = true;
        for (int k = -3; k <= 3; k++) {
            if (k != 0 && (strength[static_cast<size_t>(c + k)] > s || (strength[static_cast<size_t>(c + k)] == s && k < 0))) {
                best = false;
            }
        }
        if (best) {
            found.push_back(c);
        }
    }
    return found;
}

namespace {
    double median(std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return v.empty() ? 0.0 : v[v.size() / 2];
    }
}

Geo locate(const unsigned char * g, int w, int h, double approxLon, double approxLat) {
    Geo geo;
    geo.width = w;
    geo.height = h;
    geo.pixelsPerDegree = w / 18.0;
    // the assumption to fall back on: centred on the storm
    geo.lonAtLeft = approxLon - 9.0;
    geo.latAtTop = approxLat + 9.0 * h / static_cast<double>(w);
    const auto xs = graticule(g, w, h, true);
    const auto ys = graticule(g, w, h, false);
    // the spacing of the lines is 5 degrees
    std::vector<double> gaps;
    for (const auto * lines : {&xs, &ys}) {
        for (size_t i = 1; i < lines->size(); i++) {
            gaps.push_back((*lines)[i] - (*lines)[i - 1]);
        }
    }
    if (gaps.empty()) {
        return geo;
    }
    const double roughPerDegree = median(gaps) / 5.0;
    if (roughPerDegree < w / 40.0 || roughPerDegree > w / 5.0) {   // not a believable scale
        return geo;
    }
    // which 5 degree line each one is (from where the storm is), then the scale from a straight line through (degrees, pixels), which beats the whole pixel steps between lines
    std::vector<double> lonOf, latOf;
    for (int x : xs) {
        lonOf.push_back(5.0 * std::round((approxLon + (x + 0.5 - w / 2.0) / roughPerDegree) / 5.0));
    }
    for (int y : ys) {
        latOf.push_back(5.0 * std::round((approxLat - (y + 0.5 - h / 2.0) / roughPerDegree) / 5.0));
    }
    const auto slope = [] (const std::vector<double>& degrees, const std::vector<int>& pixels, double sign, double& out) {
        if (degrees.size() < 2) {
            return false;
        }
        double meanD = 0.0, meanP = 0.0;
        for (size_t i = 0; i < degrees.size(); i++) {
            meanD += degrees[i];
            meanP += pixels[i] + 0.5;
        }
        meanD /= static_cast<double>(degrees.size());
        meanP /= static_cast<double>(degrees.size());
        double cov = 0.0, var = 0.0;
        for (size_t i = 0; i < degrees.size(); i++) {
            cov += (degrees[i] - meanD) * (pixels[i] + 0.5 - meanP);
            var += (degrees[i] - meanD) * (degrees[i] - meanD);
        }
        out = var > 0.0 ? sign * cov / var : 0.0;
        return var > 0.0;
    };
    double sx = 0.0, sy = 0.0, perDegree = roughPerDegree;
    const bool haveX = slope(lonOf, xs, 1.0, sx), haveY = slope(latOf, ys, -1.0, sy);
    if (haveX && haveY) {
        perDegree = (sx + sy) / 2.0;
    } else if (haveX) {
        perDegree = sx;
    } else if (haveY) {
        perDegree = sy;
    }
    if (std::abs(perDegree - roughPerDegree) > roughPerDegree * 0.05) {   // the fit and the spacing disagree: something was misread
        return geo;
    }
    std::vector<double> lonLeft, latTop;
    for (size_t i = 0; i < xs.size(); i++) {
        lonLeft.push_back(lonOf[i] - (xs[i] + 0.5) / perDegree);
    }
    for (size_t i = 0; i < ys.size(); i++) {
        latTop.push_back(latOf[i] + (ys[i] + 0.5) / perDegree);
    }
    // each line gives the same corner if it was read right: keep them only if they agree
    const auto agrees = [] (const std::vector<double>& v, double m) {
        return std::all_of(v.begin(), v.end(), [m] (double d) { return std::abs(d - m) < 0.3; });
    };
    if (lonLeft.empty() || latTop.empty() || !agrees(lonLeft, median(lonLeft)) || !agrees(latTop, median(latTop))) {
        return geo;
    }
    geo.fromGrid = true;
    geo.pixelsPerDegree = perDegree;
    geo.lonAtLeft = median(lonLeft);
    geo.latAtTop = median(latTop);
    return geo;
}
}
