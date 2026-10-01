// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/SoundingPrecip.h"
#include <algorithm>
#include <vector>
#include "sounding/SoundingThermo.h"

namespace {
    using SoundingThermo::isMissing;

    struct Source {
        double pressure{-9999.0};
        int phase{-1};
        double temp{-9999.0};
    };

    // init_phase(): the precipitation source is the highest 50 mb layer below 5 km AGL with RH over 80% at its top and its
    // bottom (no vertical-velocity profile here, so SHARPpy's saturation search is the one used); its phase follows the
    // temperature at its middle.
    Source initPhase(const SoundingProfile& profile) {
        std::vector<double> topPressures;   // pressure of each level below 5 km AGL that is over 80% RH
        for (size_t i = 0; i < profile.size(); i += 1) {
            const double agl = profile.hght[i] - profile.sfcHght();
            if (agl >= 5000.0 || agl < 0.0) {
                continue;
            }
            if (isMissing(profile.tmpc[i]) || isMissing(profile.dwpc[i])) {
                continue;
            }
            if (SoundingThermo::relativeHumidityPct(profile.pres[i], profile.tmpc[i], profile.dwpc[i]) > 80.0) {
                topPressures.push_back(profile.pres[i]);
            }
        }
        int highest = -1;
        for (size_t j = 0; j < topPressures.size(); j += 1) {
            const double bottom = topPressures[j] + 50.0;
            const double t = profile.interpTemp(bottom);
            const double td = profile.interpDwpt(bottom);
            if (!isMissing(t) && !isMissing(td) && SoundingThermo::relativeHumidityPct(bottom, t, td) > 80.0) {
                highest = std::max(highest, static_cast<int>(j));   // pressure falls with index: the largest is the highest layer
            }
        }
        Source source;
        if (highest < 0) {
            return source;   // no source layer
        }
        source.pressure = topPressures[static_cast<size_t>(highest)] + 50.0 - 25.0;
        source.temp = profile.interpTemp(source.pressure);
        if (isMissing(source.temp)) {
            return source;
        }
        if (source.temp > 0.0) {
            source.phase = 0;
        } else if (source.temp > -5.0) {
            source.phase = 1;   // freezing rain
        } else if (source.temp > -9.0) {
            source.phase = 1;   // freezing rain / snow mix
        } else {
            source.phase = 3;
        }
        return source;
    }

    // posneg_temperature(): from the source level down to the surface, the energy (J/kg) of the warm and the cold parts of
    // the temperature profile, counted once a warm layer has been found; both are 0 unless a warm layer is followed by a
    // cold one.
    void posNeg(const SoundingProfile& profile, double start, double& positive, double& negative) {
        positive = 0.0;
        negative = 0.0;
        if (isMissing(profile.interpTemp(500.0)) && isMissing(profile.interpTemp(850.0))) {
            return;
        }
        const int lowest = profile.sfc;
        const double upper = start > 0.0 ? start : 500.0;
        int upperIndex = 0;
        for (int i = 0; i < static_cast<int>(profile.size()); i += 1) {
            if (profile.pres[static_cast<size_t>(i)] > upper) {
                upperIndex = i;   // the last level with pressure above the source level
            }
        }
        double height1 = profile.interpHght(upper);
        double temp1 = profile.interpTemp(upper);
        bool warmLayer = false;
        bool coldLayer = false;
        double totalPositive = 0.0;
        double totalNegative = 0.0;
        for (int i = upperIndex; i >= lowest; i -= 1) {
            const double pressure2 = profile.pres[static_cast<size_t>(i)];
            const double height2 = profile.hght[static_cast<size_t>(i)];
            const double temp2 = profile.interpTemp(pressure2);
            if (isMissing(temp1) || isMissing(temp2) || isMissing(height1)) {
                temp1 = temp2;
                height1 = height2;
                continue;
            }
            const double deficit1 = (0.0 - temp1) / (temp1 + SoundingThermo::zeroCelsiusK);
            const double deficit2 = (0.0 - temp2) / (temp2 + SoundingThermo::zeroCelsiusK);
            const double layerEnergy = 9.8 * (deficit1 + deficit2) / 2.0 * (height2 - height1);
            if (temp2 > 0.0 && !warmLayer) {
                warmLayer = true;
            }
            if (temp2 < 0.0 && warmLayer && !coldLayer) {
                coldLayer = true;
            }
            if (warmLayer) {
                (layerEnergy > 0.0 ? totalPositive : totalNegative) += layerEnergy;
            }
            temp1 = temp2;
            height1 = height2;
        }
        if (warmLayer && coldLayer) {
            positive = totalPositive;
            negative = totalNegative;
        }
    }
}

SoundingPrecip::Result SoundingPrecip::bestGuess(const SoundingProfile& profile) {
    Result result;
    if (profile.size() < 3) {
        return result;
    }
    const auto source = initPhase(profile);
    result.phase = source.phase;
    result.sourcePressure = source.pressure;
    result.sourceTemp = source.temp;
    result.surfaceTempC = profile.tmpc[static_cast<size_t>(profile.sfc)];
    posNeg(profile, source.phase >= 0 ? source.pressure : -1.0, result.positiveArea, result.negativeArea);
    const double sfc = result.surfaceTempC;
    const double tpos = result.positiveArea;
    const double tneg = result.negativeArea;

    // best_guess_precip(), case by case in SHARPpy's order
    if (source.phase < 0) {
        result.type = "None";
    } else if (source.phase == 0 && tneg >= 0.0 && sfc > 0.0) {
        result.type = "Rain";                            // always too warm
    } else if (source.phase == 3 && tpos <= 0.0 && sfc <= 0.0) {
        result.type = "Snow";                            // always too cold
    } else if (source.phase == 1 && tpos <= 0.0 && sfc > 0.0) {
        result.type = "Rain";                            // freezing-rain source, too warm at the surface
    } else if (source.phase == 1 && tpos <= 0.0 && sfc <= 0.0) {
        // non-snow source, always too cold
        if (profile.toAgl(profile.interpHght(source.pressure)) >= 3000.0) {
            result.type = source.temp <= -4.0 ? "Sleet and Snow" : "Sleet";
        } else {
            result.type = "Freezing Rain/Drizzle";
        }
    } else if (source.phase == 3 && tpos <= 0.0 && sfc > 0.0) {
        result.type = sfc > 4.0 ? "Rain" : "Snow";       // snow source, warm surface
    } else if (tpos > 0.0) {
        // a warm layer: sleet when the cold layer below it is deep enough to refreeze the drops
        const double needed = 0.62 * tpos + 60.0;
        if (-tneg > needed) {
            result.type = "Sleet";
        } else {
            result.type = sfc <= 0.0 ? "Freezing Rain" : "Rain";
        }
    } else {
        result.type = "Unknown";
    }
    return result;
}
