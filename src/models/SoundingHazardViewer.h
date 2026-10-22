// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGHAZARDVIEWER_H
#define SOUNDINGHAZARDVIEWER_H

#include <memory>
#include <string>
#include <QWidget>
#include "sounding/SoundingAnalysis.h"
#include "sounding/SoundingHazard.h"
#include "sounding/SoundingProfile.h"
#include "ui/Window.h"

class HazardChart;

// SPC's flow chart of the possible hazard types for a sounding (the tornado column on the left, severe storms, flash flooding, blizzard and heat on the right), drawn with the values of this sounding
// at every test and the path the walk took. The flash flood test needs the site's precipitable water climatology: it is read in the background (the reanalysis' mean and spread at the point and the
// day) when the point is known and the spread has been built (Settings), else that test says it has none.
class SoundingHazardViewer : public Window {
public:
    // lat / lon: where the sounding is (for the climatology), validTime: "yyMMdd/HHmm" or ""; both optional
    SoundingHazardViewer(Window * parent, const SoundingProfile& profile, const SoundingAnalysis& analysis, double lat, double lon, const std::string& title);

private:
    void closeEventCustom() override { closed = true; }
    SoundingProfile profile;
    SoundingAnalysis analysis;
    double lat, lon;
    HazardChart * chart{};
    int generation{0};
    bool closed{false};
};

#endif  // SOUNDINGHAZARDVIEWER_H
