// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYREFS_H
#define UTILITYREFS_H

#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <QColor>
#include <QString>
#include "models/UtilityGrib.h"

using std::string;
using std::vector;

// REFS ("RRFS Ensemble" raw members + REFS's own blended `ensprod` derived
// products) viewer support - see docs/refs-viewer-plan.md. Stage 0: one
// working ensemble-mean render through the `ensprod` pipeline, reusing
// UtilityGrib's region table / GDAL shell-out pattern / idx byte-range
// parser wholesale, proving the data path before the member-comparison work
// (paintball, plume) in later stages.
class UtilityRefs {
public:
    // Stage 0's field table: REFS ensemble-MEAN rows only, reusing
    // UtilityGrib::Field's shape verbatim - its `product` slot is
    // repurposed here to mean the `ensprod` product type ("mean") rather
    // than "2dfld"/"prslev", since that's what actually selects which
    // `refs.*` file a field's `idxMatch` is looked up in.
    static const vector<UtilityGrib::Field> fields;

    static vector<string> fieldLabels();
    // Region picker - identical list/geometry to UtilityGrib's own
    // regions() (CONUS, "My Area", SPC-meso sectors). REFS runs at the same
    // 3km/2.5km resolution as deterministic RRFS (confirmed against SCN
    // 26-48, see docs/refs-viewer-plan.md) so there is no resolution-based
    // reason to trim this the way a coarser ensemble might need to.
    static vector<string> regions();
    // REFS-only run cycles (00/06/12/18z) - a thin pass-through to
    // UtilityGrib::synopticRunOptions(), shared with UtilitySevereIndices.
    static vector<std::pair<string, string>> runOptions();
    // REFS runs to F60 only (vs RRFS deterministic's F84) - not a
    // UtilityGrib::forecastHours() pass-through, the max hour differs.
    static vector<string> forecastHours();

    // Production render: ensemble-mean fill for the given field/region/hour
    // /run, cached, with a ".grid" hover sidecar like UtilityGrib::render().
    // Returns the PNG path, or "" on failure.
    // `threshold` applies only to the threshold-driven rows (Paintball /
    // Member Probability / REFS Probability - see usesThreshold()); NaN =
    // that row's default. REFS Probability snaps to the nearest published
    // band (the status line reports the one actually used).
    static string render(int fieldIndex, int regionIndex, const string& forecastHour, const string& runId,
                         string& status, double& dataMin, double& dataMax, string& samplePath,
                         double threshold = std::numeric_limits<double>::quiet_NaN());

    static bool usesThreshold(int fieldIndex);
    static double defaultThreshold(int fieldIndex);
    static string thresholdUnits(int fieldIndex);

private:
    static string cacheDir();
    static string renderPaintball(const UtilityGrib::Field& field, int regionIndex, double threshold,
                                  const string& dateStr, const string& cycle, int forecastHourInt,
                                  const string& binDir, string& status);
    static string renderMemberProbability(const UtilityGrib::Field& field, int regionIndex, double threshold,
                                          const string& dateStr, const string& cycle, int forecastHourInt,
                                          const string& binDir, string& status, double& dataMin,
                                          double& dataMax, string& samplePath);
    static bool warpMember(const string& memberKey, int member, const string& dateStr, const string& cycle,
                           int forecastHourInt, const UtilityGrib::Bbox& box, const QString& bin,
                           const QString& warpPath, string& status);
    static void drawLegend(const QString& pngPath, const QString& title,
                           const vector<std::pair<QString, QColor>>& swatches, bool probabilityBar);
    static string finishRender(const QString& warpPath, const string& colorMap, const QString& tag,
                               const UtilityGrib::Bbox& box, const QString& bin, const QString& pngPath,
                               string& status, double& dataMin, double& dataMax, string& samplePath,
                               const QString& legendTitle, bool probabilityLegend);
    static bool fetchFieldGrib(const UtilityGrib::Field& field, const string& dateStr, const string& cycle,
                               int forecastHourInt, QString& gribPathOut, string& status,
                               const string& alsoContains = "");
};

#endif  // UTILITYREFS_H
