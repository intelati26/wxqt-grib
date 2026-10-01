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

    // Panel pickers: the flat field list is grouped into "kinds" (REFS
    // mean/spread/PMM, a single RRFS Ensemble member, paintball, member
    // probability, REFS probability). Within a kind the panel offers one
    // entry per variable; for the member kind those entries are the
    // member-1 rows and memberFieldIndex() maps to the chosen member.
    static vector<string> kindLabels();
    static int kindOf(int fieldIndex);
    static vector<int> kindFieldIndices(int kind);
    static string variableLabel(int fieldIndex);   // field label without the kind prefix
    static bool isMemberField(int fieldIndex);
    static int memberOf(int fieldIndex);           // 1-based, 0 if not a member row
    static int memberFieldIndex(int anyMemberRow, int member);
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

    // Run-to-run change map: the same valid time from the run `hoursBack` hours earlier is subtracted. Plain rows
    // only (not paintball / member probability). Same return / sidecar contract as render().
    static string renderDifference(int fieldIndex, int regionIndex, const string& forecastHour, const string& runId,
                                   int hoursBack, string& status, double& dataMin, double& dataMax, string& samplePath,
                                   double threshold = std::numeric_limits<double>::quiet_NaN());

    // ---- Stage 5 (point graph) support ----
    // What a panel's field means for the per-member plume chart: the
    // per-member field key it is built on ("refc", "tmp2m", ...), a display
    // label, units, and (for threshold rows) the threshold to draw as a
    // reference line. False for REFS-only rows (mean/spread/pmmn/REFS prob),
    // which have no per-member data.
    struct MemberBasis {
        string memberKey;
        string label;
        string units;
        bool hasThreshold{false};
        double threshold{0.0};
    };
    static bool memberBasis(int fieldIndex, double panelThreshold, MemberBasis& basis);
    static constexpr int memberCount = 5;
    // Each RRFS Ensemble member's value at (lon, lat) for one forecast hour
    // (index 0 = member 1). NaN where a member is unavailable or the point
    // is outside its domain. Blocking - run it off the UI thread.
    static vector<double> memberPointValues(const string& memberKey, const string& dateStr, const string& cycle,
                                            int forecastHourInt, double lon, double lat, string& status);
    static QColor memberColor(int member);   // 1-based, matches the paintball key

    static bool usesThreshold(int fieldIndex);
    static double defaultThreshold(int fieldIndex);
    static string thresholdUnits(int fieldIndex);

private:
    struct Selection;   // defined in UtilityRefs.cpp
    static Selection selectField(int fieldIndex, double threshold, int forecastHourInt);
    static bool plainRaster(const Selection& selection, int regionIndex, const string& dateStr, const string& cycle,
                            int forecastHourInt, const string& binDir, const QString& tag, QString& rasterPath,
                            string& status);
    static string cacheDir();
    static string renderPaintball(const UtilityGrib::Field& field, int regionIndex, double threshold,
                                  const string& dateStr, const string& cycle, int forecastHourInt,
                                  const string& binDir, string& status, string& samplePath);
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
