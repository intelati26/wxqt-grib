// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingProfile.h"
#include <cmath>
#include <cstdlib>
#include <sstream>
#include "sounding/SoundingThermo.h"

using SoundingThermo::isMissing;

namespace {
    constexpr double degToRad = 3.14159265358979323846 / 180.0;

    std::vector<std::string> splitCsv(const std::string& line) {
        std::vector<std::string> parts;
        std::string current;
        for (const char c : line) {
            if (c == ',') {
                parts.push_back(current);
                current.clear();
            } else {
                current += c;
            }
        }
        parts.push_back(current);
        return parts;
    }
}

bool SoundingProfile::parseSpcText(const std::string& text, SoundingProfile& out, std::string& error) {
    out = SoundingProfile{};
    std::istringstream stream{text};
    std::string line;
    bool inTitle = false;
    bool inRaw = false;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("%TITLE%", 0) == 0) {
            inTitle = true;
            continue;
        }
        if (line.rfind("%RAW%", 0) == 0) {
            inTitle = false;
            inRaw = true;
            continue;
        }
        if (line.rfind("%END%", 0) == 0) {
            break;
        }
        if (inTitle) {
            std::istringstream words{line};
            std::string first;
            std::string second;
            if (out.station.empty() && (words >> first >> second)) {
                out.station = first;
                out.validTime = second;
            }
            continue;
        }
        if (!inRaw) {
            continue;
        }
        const auto parts = splitCsv(line);
        if (parts.size() < 6) {
            continue;
        }
        double fields[6];
        bool ok = true;
        for (size_t i = 0; i < 6; i += 1) {
            char * end = nullptr;
            fields[i] = std::strtod(parts[i].c_str(), &end);
            if (end == parts[i].c_str()) {
                ok = false;
            }
        }
        if (!ok) {
            continue;
        }
        out.pres.push_back(fields[0]);
        out.hght.push_back(fields[1]);
        out.tmpc.push_back(fields[2]);
        out.dwpc.push_back(fields[3]);
        out.wdir.push_back(fields[4]);
        out.wspd.push_back(fields[5]);
    }
    if (out.pres.size() < 3) {
        error = "no sounding levels found";
        return false;
    }
    out.finalize();
    bool haveSurface = false;
    for (size_t i = 0; i < out.size(); i += 1) {
        if (!isMissing(out.tmpc[i]) && !isMissing(out.dwpc[i])) {
            haveSurface = true;
            break;
        }
    }
    if (!haveSurface) {
        error = "no level with both temperature and dewpoint";
        return false;
    }
    return true;
}

void SoundingProfile::finalize() {
    const size_t n = pres.size();
    u.assign(n, SoundingThermo::missing);
    v.assign(n, SoundingThermo::missing);
    vtmp.assign(n, SoundingThermo::missing);
    thetae.assign(n, SoundingThermo::missing);
    wetbulb.assign(n, SoundingThermo::missing);
    tempSeries = dwptSeries = vtmpSeries = thetaeSeries = hghtSeries = uSeries = vSeries = Series{};
    sfc = 0;
    bool foundSfc = false;
    for (size_t i = 0; i < n; i += 1) {
        const bool presOk = !isMissing(pres[i]) && pres[i] > 0.0;
        if (!presOk) {
            continue;
        }
        const double lp = std::log10(pres[i]);
        if (!isMissing(wdir[i]) && !isMissing(wspd[i])) {
            u[i] = -wspd[i] * std::sin(wdir[i] * degToRad);
            v[i] = -wspd[i] * std::cos(wdir[i] * degToRad);
            uSeries.logp.push_back(lp);
            uSeries.value.push_back(u[i]);
            vSeries.logp.push_back(lp);
            vSeries.value.push_back(v[i]);
        }
        if (!isMissing(tmpc[i])) {
            tempSeries.logp.push_back(lp);
            tempSeries.value.push_back(tmpc[i]);
        }
        if (!isMissing(dwpc[i])) {
            dwptSeries.logp.push_back(lp);
            dwptSeries.value.push_back(dwpc[i]);
        }
        if (!isMissing(tmpc[i]) && !isMissing(dwpc[i])) {
            vtmp[i] = SoundingThermo::virtualTemp(pres[i], tmpc[i], dwpc[i]);
            vtmpSeries.logp.push_back(lp);
            vtmpSeries.value.push_back(vtmp[i]);
            thetae[i] = SoundingThermo::thetaE(pres[i], tmpc[i], dwpc[i]);
            wetbulb[i] = SoundingThermo::wetBulb(pres[i], tmpc[i], dwpc[i]);
            thetaeSeries.logp.push_back(lp);
            thetaeSeries.value.push_back(thetae[i]);
            if (!foundSfc && !isMissing(hght[i])) {
                sfc = static_cast<int>(i);
                foundSfc = true;
            }
        }
        if (!isMissing(hght[i])) {
            hghtSeries.logp.push_back(lp);
            hghtSeries.value.push_back(hght[i]);
        }
    }
    // height -> pressure needs strictly increasing heights
    hghtAsc.clear();
    logpAtHght.clear();
    for (size_t i = 0; i < hghtSeries.value.size(); i += 1) {
        if (hghtAsc.empty() || hghtSeries.value[i] > hghtAsc.back()) {
            hghtAsc.push_back(hghtSeries.value[i]);
            logpAtHght.push_back(hghtSeries.logp[i]);
        }
    }
}

// linear in log10(p); series are ordered from high pressure (low log p ... i.e. large) downward
double SoundingProfile::interpLogP(const Series& series, double pMb) {
    const size_t n = series.logp.size();
    if (n < 2 || pMb <= 0.0) {
        return SoundingThermo::missing;
    }
    const double lp = std::log10(pMb);
    if (lp > series.logp.front() + 1e-12 || lp < series.logp.back() - 1e-12) {
        return SoundingThermo::missing;
    }
    for (size_t i = 0; i + 1 < n; i += 1) {
        const double a = series.logp[i];
        const double b = series.logp[i + 1];
        if (lp <= a + 1e-12 && lp >= b - 1e-12) {
            if (std::fabs(a - b) < 1e-12) {
                return series.value[i];
            }
            return series.value[i] + (lp - a) / (b - a) * (series.value[i + 1] - series.value[i]);
        }
    }
    return SoundingThermo::missing;
}

double SoundingProfile::interpHght(double pMb) const { return interpLogP(hghtSeries, pMb); }
double SoundingProfile::interpTemp(double pMb) const { return interpLogP(tempSeries, pMb); }
double SoundingProfile::interpDwpt(double pMb) const { return interpLogP(dwptSeries, pMb); }
double SoundingProfile::interpVtmp(double pMb) const { return interpLogP(vtmpSeries, pMb); }

double SoundingProfile::interpThetae(double pMb) const { return interpLogP(thetaeSeries, pMb); }

bool SoundingProfile::interpComponents(double pMb, double& uOut, double& vOut) const {
    uOut = interpLogP(uSeries, pMb);
    vOut = interpLogP(vSeries, pMb);
    return !isMissing(uOut) && !isMissing(vOut);
}

double SoundingProfile::interpPresAtHght(double hMsl) const {
    const size_t n = hghtAsc.size();
    if (n < 2 || hMsl < hghtAsc.front() || hMsl > hghtAsc.back()) {
        return SoundingThermo::missing;
    }
    for (size_t i = 0; i + 1 < n; i += 1) {
        if (hMsl >= hghtAsc[i] && hMsl <= hghtAsc[i + 1]) {
            const double f = (hMsl - hghtAsc[i]) / (hghtAsc[i + 1] - hghtAsc[i]);
            return std::pow(10.0, logpAtHght[i] + f * (logpAtHght[i + 1] - logpAtHght[i]));
        }
    }
    return SoundingThermo::missing;
}
