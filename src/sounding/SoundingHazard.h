// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SOUNDINGHAZARD_H
#define SOUNDINGHAZARD_H

#include <string>
#include <vector>
#include "sounding/SoundingAnalysis.h"
#include "sounding/SoundingProfile.h"

// The "possible hazard type" of SHARPpy / SPC's flow chart (sharptab/watch_type.py: possible_watch): the sounding's parameters are walked through a chain of tests, the left column for tornadoes
// (PDS TOR, TOR, MRGL TOR), the right column from the bottom for severe storms (SVR, MRGL SVR), flash flooding, blizzard and excessive heat, and NONE when nothing holds. Every test is kept with the
// value it was given, so a screen can draw the chart with the path taken and why each step passed or not. See docs/sharppy-notice.md.
namespace SoundingHazard {
    enum class State { Pass, Fail, Unknown };
    struct Condition {
        std::string text;     // "STPE >= 3"
        std::string value;    // "1.2" (what the sounding has), "--" when it has none
        State state{State::Unknown};
    };
    struct Node {
        std::string id;                  // "L1" .. "L6" (the tornado column, top down), "R1" .. "R7" (the other column, from the bottom up)
        std::vector<Condition> lines;    // each line one test (a line may hold "or" / "and" of its own)
        std::string outcome;             // what it gives when it holds: "PDS TOR", "TOR", "MRGL TOR", "SVR", "MRGL SVR", "FLASH FLOOD", "BLIZZARD", "EXCESSIVE HEAT"
        bool reached{false};             // the walk got to this node
        bool passed{false};              // and it holds
    };
    struct Input {
        double pwMeanIn{-9999.0};        // the climatological precipitable water of the site and day (inches), and its standard deviation: not every sounding has them
        double pwSigmaIn{-9999.0};
    };
    struct Result {
        std::string type{"NONE"};
        std::vector<Node> nodes;         // in the order of the walk
        bool pwKnown{false};             // the flash flood test had the climatology
    };
    Result classify(const SoundingProfile& profile, const SoundingAnalysis& analysis, const Input& input = {});
}

#endif  // SOUNDINGHAZARD_H
