// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSEVEREINDICES_H
#define UTILITYSEVEREINDICES_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QString>
#include "models/UtilityGrib.h"

using std::string;
using std::vector;

// Derived severe-weather proxy fields (SHIP, and eventually SIGTOR/STP)
// computed from RRFS fields via chained gdal_calc expressions - see
// docs/derived-severe-indices-plan.md. Segment 1: fetch and individually
// verify the raw inputs SHIP needs, through wxqt's own byte-range-fetch /
// gdalwarp pipeline, before any formula math is added. Segment 2: the
// derived intermediate grids (mixing ratio, lapse rate, shear magnitude,
// freezing level AGL) the SHIP formula itself is built from.
class UtilitySevereIndices {
public:
    // The "Parametric" combo's row table (mirrors UtilityGrib::Field) - one
    // row today (SHIP), a second appended once SIGTOR/STP's formula is
    // confirmed. colorMap is a gdaldem color-relief table, same format as
    // UtilityGrib::Field::colorMap, so IndexViewer can build a real legend
    // from it the same way GribViewer does.
    struct IndexEntry {
        string label;
        string key;
        string units;
        string colorMap;
    };
    static const vector<IndexEntry> indices;

    struct FieldCheck {
        string label;
        string key;
        string units;
        bool ok = false;
        string pngPath;    // auto-scaled grayscale PNG, "" on failure
        double dataMin = 0.0;
        double dataMax = 0.0;
        string error;
    };

    struct DerivedCheck {
        string label;
        string key;
        string units;
        string formula;    // human-readable, for the debug printout
        bool ok = false;
        string pngPath;
        double dataMin = 0.0;
        double dataMax = 0.0;
        string error;
    };

    static bool gdalAvailable();

    // Fetches every raw field SHIP's formula needs (see shipInputFields()
    // below) for one run/hour over CONUS, warps each to a common grid, and
    // writes an auto-scaled grayscale PNG per field for visual/numeric
    // sanity-checking. No formula math - purely "did the fetch work and does
    // the value range look right." runId "" means latest RRFS run.
    static vector<FieldCheck> debugFetchShipInputs(const string& forecastHour, const string& runId,
                                                    string& status);

    // Computes the four intermediate grids SHIP's formula multiplies
    // together (mixing ratio, 700-500mb lapse rate, 0-6km shear magnitude,
    // freezing level AGL) from the raw fields above, and dumps each as an
    // auto-scaled grayscale PNG for numeric/visual verification before the
    // capping/rescaling formula itself (Segment 3) is written.
    static vector<DerivedCheck> debugComputeDerivedGrids(const string& forecastHour, const string& runId,
                                                          string& status);

    struct ShipCheck {
        bool ok = false;
        string pngPath;       // colorized (SHIP's own interpretation breakpoints)
        string grayPngPath;   // auto-scaled grayscale, for raw numeric range sanity
        double dataMin = 0.0;
        double dataMax = 0.0;
        string error;
    };

    // The SHIP formula itself: caps SHEAR6/MIXR/T500, multiplies the five
    // terms, then applies the three conditional rescalers - all as one
    // gdal_calc expression (numpy's clip()/minimum()/where() are all
    // available - see Segment 2 findings). The one piece of this pipeline
    // that was genuinely untested until tried.
    static ShipCheck debugComputeShip(const string& forecastHour, const string& runId, string& status);

    // "Parametric" combo labels - just SHIP for now, STP appended later as a
    // second row once its formula is confirmed (see docs/derived-severe-indices-plan.md).
    static vector<string> indexLabels();

    // Region picker - identical list/geometry to UtilityGrib's own regions()
    // (CONUS, "My Area", SPC-meso sectors), reused rather than duplicated.
    static vector<string> regionLabels();

    // {display label, run id ("" == latest synoptic)}, filtered from
    // UtilityGrib::runOptions() down to 00/06/12/18z cycles only - SHIP's
    // inputs (HAIL, MUCAPE, shear, freezing level) simply don't exist in
    // the sub-hourly file an off-cycle run would fall back to (Segment 1
    // finding). Picking a non-synoptic run here would silently fail every
    // render, so the picker doesn't offer one.
    static vector<std::pair<string, string>> runOptions();

    // Forecast hours available for a run at the given cycle (00/06/12/18 ->
    // F01-84, matching UtilityGrib's own synoptic-cycle range - a plain
    // pass-through since SHIP only ever runs at synoptic cycles anyway).
    static vector<string> forecastHours(int cycle = -1);

    // Production render: index fill (+ HAILCAST contour overlay for SHIP
    // only) for the given index/region/hour/run, cached, with a ".grid"
    // hover sidecar like UtilityGrib::render(). indexIndex 0 = SHIP,
    // 1 = STP (fixed layer); any other value fails with "unsupported index".
    // Returns the PNG path, or "" on failure.
    static string render(int indexIndex, int regionIndex, const string& forecastHour, const string& runId,
                         string& status, double& dataMin, double& dataMax, string& samplePath);

private:
    static const vector<UtilityGrib::Field> shipInputFields;
    // STP (fixed layer)'s raw inputs not already covered by shipInputFields
    // (0-6km shear and terrain height ARE shared - see computeStpGrid()):
    // surface CAPE/CIN, 0-1km SRH, and LCL height (still geopotential/MSL
    // like every other HGT: field here, same convention SHIP's own
    // freezing-level term already relies on).
    static const vector<UtilityGrib::Field> stpInputFields;
    static string cacheDir();
    // "" (or any non-synoptic runId) resolves to the most recent synoptic
    // run via runOptions() rather than UtilityGrib::getLatestRun() (which
    // returns the latest *hourly* run - usually not synoptic). Falls back
    // to that anyway if no synoptic run can be found at all.
    static bool resolveSynopticRun(const string& runId, string& dateStr, string& cycle);
    // fetches + warps every field in `wanted` to `box`, sized via
    // UtilityGrib::mainRenderColumns(box); returns key -> warped GeoTIFF
    // path (still on disk - callers are responsible for any cleanup once
    // done reading them).
    static bool fetchAndWarpAll(const vector<UtilityGrib::Field>& wanted, const UtilityGrib::Bbox& box,
                                 const string& forecastHour, const string& runId, string& runKeyOut,
                                 string& fhr3Out, std::map<string, QString>& warpPaths, string& status);

    struct ShipGrid {
        bool ok = false;
        QString shipTifPath;         // raw (uncolorized) SHIP value grid - caller deletes
        // one raw *input* warp (not run through the formula) - gdal_calc's
        // --calc does not propagate NoData through arithmetic (an input of
        // -9999 just produces some other, arbitrary number, not -9999), so
        // shipTifPath's own values can't be used to tell true off-domain
        // nodata apart from real data. This field, still carrying -9999
        // exactly where gdalwarp put it, is what a caller needing a nodata
        // mask should build it from instead. Caller deletes.
        QString nodataRefPath;
        QString hailContourBufPath;  // smoothed contour polygons, ready to gdal_rasterize - "" if none; caller deletes
        QString hailContourRawPath;  // unsimplified geojson, for label placement - "" if none; caller deletes
        double dataMin = 0.0;
        double dataMax = 0.0;
        string runKey;
        string fhr3;
        string error;
    };
    // computes the SHIP formula grid and the HAILCAST contour geometry for
    // `box` - the part debugComputeShip() and render() share verbatim.
    // Callers own colorizing shipTifPath and burning/labelling the contour.
    static ShipGrid computeShipGrid(const UtilityGrib::Bbox& box, const string& forecastHour, const string& runId);

    struct StpGrid {
        bool ok = false;
        QString tifPath;        // raw (uncolorized) STP value grid - caller deletes
        QString nodataRefPath;  // untouched input warp for nodata masking - see ShipGrid's own comment
        double dataMin = 0.0;
        double dataMax = 0.0;
        string runKey;
        string fhr3;
        string error;
    };
    // STP (fixed layer, Thompson et al. 2003): (SBCAPE/1500) x LCL-term x
    // (SRH1/150) x shear-term x CIN-term, capped per SPC's published
    // breakpoints (spc.noaa.gov/exper/mesoanalysis/help/help_stor.html).
    // No contour overlay (unlike SHIP/HAILCAST) - STP is the whole render.
    static StpGrid computeStpGrid(const UtilityGrib::Bbox& box, const string& forecastHour, const string& runId);
};

#endif  // UTILITYSEVEREINDICES_H
