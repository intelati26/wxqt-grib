// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSVERIFY_H
#define MRMSVERIFY_H

#include <vector>
#include <QImage>
#include "gfs/GfsGrid.h"
#include "mrms/UtilityMrms.h"

// A model's reflectivity (dBZ) against MRMS's at one time: the MRMS scan is put on the model's grid (the highest value of the MRMS cells inside each model cell),
// then the cells in a box are counted against a threshold (hits, misses, false alarms) and the Fractions Skill Score is worked out for neighbourhoods of
// a few sizes (square windows, in km), which does not punish a storm that is a few miles off the way the cell by cell scores do.
namespace MrmsVerify {
    struct Box {
        double south{20.0}, north{55.0}, west{-130.0}, east{-60.0};
    };

    struct Contingency {
        long hits{0}, misses{0}, falseAlarms{0}, correctNegatives{0};
        double pod() const;    // probability of detection: the observed echoes the model had
        double far() const;    // false alarm ratio: the model's echoes that were not observed
        double csi() const;    // critical success index
        double bias() const;   // model's area over the observed area (1 = the same)
    };

    struct Score {
        double dbz{0.0};
        Contingency table;
        std::vector<double> fss;   // one per radius asked for; NaN when neither field has the threshold anywhere
    };

    struct Result {
        std::vector<Score> scores;
        std::vector<double> radiiKm;
        long cells{0};   // the cells compared
    };

    // the highest MRMS value in each model cell (NaN outside MRMS's area); `frames` more than one: the highest of all of them (the maximum of an hour)
    GfsGrid::Grid observed(const std::vector<UtilityMrms::Frame>& frames, const GfsGrid::Grid& model);

    Result score(const GfsGrid::Grid& model, const GfsGrid::Grid& observed, const Box& box, const std::vector<double>& thresholds, const std::vector<double>& radiiKm);

    // the model, MRMS and the agreement (hit green, miss red, false alarm blue) at a threshold, side by side with the state lines
    QImage picture(const GfsGrid::Grid& model, const GfsGrid::Grid& observed, const Box& box, double threshold, const QString& modelTitle, const QString& observedTitle, int panelWidth);
}

#endif  // MRMSVERIFY_H
