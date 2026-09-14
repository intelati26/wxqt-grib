// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYGRIB_H
#define UTILITYGRIB_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QString>

using std::string;
using std::vector;

// Minimal GRIB2 viewer support for the operational RRFS.
// GRIB2 decode, reprojection and colorization are delegated to the GDAL command
// line tools; this class only locates the data, byte-range downloads a single
// record, and drives gdalwarp / gdaldem / gdal_rasterize.
class UtilityGrib {
public:
    struct Field {
        string label;      // shown in the combo box
        string key;        // short slug for cache filenames
        string units;      // legend units, e.g. "C" or "dBZ"
        string idxMatch;   // token to find in the .idx (e.g. ":TMP:2 m above ground:")
        string colorMap;   // gdaldem color-relief table (newline separated)
        string product = "2dfld";   // NOMADS product component: "2dfld" or "prslev"
        int contourInterval = 0;    // if > 0, overlay contour lines at this interval
        string contourIdxMatch = "";  // if set, contour this OTHER record (derived product)
        bool windBarbs = false;       // if set, overlay 10m wind barbs
    };

    struct Bbox {
        double west;
        double south;
        double east;
        double north;
    };

    static const vector<Field> fields;

    // Shared gdaldem color-relief tables, exposed (not just anon-namespace
    // private) so a sibling RRFS-derived viewer rendering the same physical
    // quantity (e.g. UtilityRefs's ensemble-mean 2m temp / reflectivity)
    // gets the identical visual scale for free instead of keeping its own
    // copy of a nontrivial table.
    static const string tempColorMap;
    static const string reflColorMap;

    static vector<string> fieldLabels();
    static vector<string> forecastHours(int cycle = -1);   // cycle 0/6/12/18 -> out to F84
    static vector<string> regions();
    // {display label, run id ("" == latest)}, newest first, "Latest" at index 0
    static vector<std::pair<string, string>> runOptions();
    // runOptions() filtered to synoptic (00/06/12/18z) cycles only, "Latest"
    // re-added at index 0 pointing at "" (not runOptions()'s own hourly
    // "Latest"). Shared by any RRFS-derived product whose inputs only exist
    // at synoptic cycles (UtilitySevereIndices' SHIP/STP, UtilityRefs) so
    // each doesn't keep its own copy of this filter.
    static vector<std::pair<string, string>> synopticRunOptions();
    // "" (or any non-synoptic runId) resolves to the most recent synoptic
    // run via synopticRunOptions(), for the same sharing reason - falls
    // back to getLatestRun() (which may not be synoptic) if none is found.
    static bool resolveSynopticRun(const string& runId, string& dateStr, string& cycle);
    static bool gdalAvailable();

    // "para" or "prod" - the current NOMADS pre/post-cutover stream name.
    // Exposed so a sibling product building its own URL against a
    // differently-shaped NOMADS path (UtilityRefs, `refs.*`/`rrfsens.*`
    // rather than `rrfs.*`) flips at the same single edit point
    // (`rrfsStream` in UtilityGrib.cpp) rather than needing its own copy of
    // the cutover flag.
    static string dataStream();

    // Runs the full pipeline for the latest available run and returns the path to
    // a rendered PNG, or "" on failure. status is set to a human readable line
    // (the run/valid time on success, the reason on failure). dataMin/dataMax are
    // set to the value range actually present in the rendered area (both 0 if
    // unknown), for trimming the legend. samplePath is set to a cached ".grid"
    // sidecar (gdal XYZ dump of the field, display units) covering the same
    // extent as the PNG, for point value read-out on hover; "" if unavailable.
    static string render(int fieldIndex, int regionIndex, const string& forecastHour,
                         const string& runId, string& status, double& dataMin, double& dataMax,
                         string& samplePath);

    // Latest resolved RRFS run ("YYYYMMDD", "HH"). Exposed so another product's
    // renderer (e.g. UtilitySpcPost) can align its own forecast valid time onto
    // RRFS's clock rather than rendering RRFS output directly.
    static bool getLatestRun(string& dateStr, string& cycle);

    // Renders a single RRFS field ("temp" -> 2m Temperature, "reflectivity" ->
    // Composite Reflectivity) warped to an arbitrary bbox, with none of
    // render()'s contours/station plots/legend-range/hover sidecar - meant as
    // a plain background layer under another product's own (partly
    // transparent) overlay. Returns the PNG path, or "" on failure.
    static string renderBackground(const string& kind, const Bbox& bbox, const string& forecastHour,
                                    const string& runId, string& status);

    // Byte-range fetches a single GRIB2 record matching field.idxMatch (plain
    // hourly file, falling back to the sub-hourly file for in-between hours),
    // caching it under gribPathOut. Exposed so other RRFS-derived products
    // (e.g. UtilitySevereIndices) can pull individual fields without
    // duplicating the plain/subh fallback logic a third time.
    static bool fetchFieldSlice(const Field& field, const string& dateStr, const string& cycle,
                                 const string& forecastHour, string& gribPathOut);

    // Batched version of fetchFieldSlice() for callers wanting several
    // fields at once (e.g. UtilitySevereIndices' ~13 SHIP inputs). The .idx
    // already gives every field's exact byte span, so fields whose spans
    // sit close together in the file (within a small waste threshold) share
    // one ranged GET instead of one each - no GRIB2 parsing needed, each
    // field's own slice is just cut from the shared download at the same
    // offsets idxByteRange() already computed. Populates gribPaths with
    // key -> cached grib2 path for every field that resolved successfully
    // (via either the plain or sub-hourly file, same fallback as the
    // single-field version); a field missing from both simply has no entry.
    static void fetchFieldSlices(const vector<Field>& fields, const string& dateStr, const string& cycle,
                                  const string& forecastHour, std::map<string, string>& gribPaths);

    // Draws each contour's value along its line onto the finished PNG
    // (in-place). decimals/suffix control the label text - 0/"" for a
    // rounded height in gpm (render()'s own contours), 2/"\"" for a hail
    // diameter in inches. Exposed for the same reason as fetchFieldSlice -
    // so another RRFS-derived product's contour overlay doesn't need its
    // own copy of this.
    static void labelContours(const QString& pngPath, const QString& geoJsonPath, const Bbox& box, bool green,
                               int decimals = 0, const QString& suffix = "");

    // Draws city name labels onto the finished PNG in place - biggest
    // population first, greedily spaced (same min-pixel-gap approach as the
    // station-plot picker), sourced from `CitiesExtended` (already bundled
    // for the Nexrad radar screen, previously parsed population and threw
    // it away - now used for label priority).
    static void drawCityLabels(const QString& pngPath, const Bbox& box);

    // Bbox for one of regions()'s entries ("My Area" resolves against the
    // user's current location at call time). Exposed so another RRFS-derived
    // product can reuse the same region list/geometry instead of keeping its
    // own copy - UtilitySevereIndices does this for its region picker.
    static Bbox regionBbox(int regionIndex);

    // One output column per native ~3km RRFS grid cell ("1 pixel per 3km
    // area") - true 1:1 resolution for a bbox, matching the grid's own
    // density exactly (more is wasted interpolation, less throws away real
    // detail). Exposed so other RRFS-derived renderers (UtilitySpcPost,
    // UtilitySevereIndices) share one implementation instead of each
    // keeping their own copy of the geometry math.
    static int nativeGridColumns(const Bbox& box);

    // nativeGridColumns(), stepped down for CONUS/NA-sized domains (span >
    // 30 degrees) as a deliberate speed/file-size win - the eye can't
    // resolve native-resolution detail at that zoom anyway. This is the
    // value actually used for a main visual render's -ts.
    static int mainRenderColumns(const Bbox& box);

    // County/highway line overlays. Both reuse data already bundled for the
    // Nexrad radar screen's geometry-overlay system (county.bin, hwv4.bin -
    // see RadarGeometry.cpp) rather than sourcing new boundary data; same
    // binary format `stateLinesGeoJson()` already parses. Exposed (unlike
    // that private method) so `UtilitySpcPost` calls these directly instead
    // of keeping a third copy.
    static string countyLinesGeoJson();   // path; generated once from the bundled county.bin
    static string highwayLinesGeoJson();  // path; generated once from the bundled hwv4.bin

    // Same reasoning, three more geometry-overlay resources already bundled
    // for the Nexrad radar screen and otherwise unused elsewhere: lakes,
    // and the Canada/Mexico border+province/state lines (relevant for the
    // Northwest/Great Lakes/North Central and Southwest/South Central
    // sectors respectively, which actually reach those borders).
    static string lakeLinesGeoJson();     // path; generated once from the bundled lakesv3.bin
    static string canadaLinesGeoJson();   // path; generated once from the bundled ca.bin
    static string mexicoLinesGeoJson();   // path; generated once from the bundled mx.bin

    // State lines / NWS CWAs. Made public (was private) so `UtilitySpcPost`
    // can call these directly instead of keeping its own copies - it did,
    // pre-dating the county/highway work above; folded into the same
    // upstream-sharing pass rather than left as the one pair still forked.
    static string stateLinesGeoJson();   // path; generated once from the bundled statev2.bin
    static string cwaLinesGeoJson();     // path; extracted once from the bundled resource

    // Directory holding the GDAL CLI tools (gdalwarp et al.), or "" if not
    // found on PATH. Public (was private) so sibling RRFS-derived products
    // (UtilitySevereIndices, UtilityRefs) share this lookup instead of each
    // keeping an identical copy - `gdalAvailable()` above is just the bool
    // form of this for a plain feature-gate check.
    static string gdalBinDir();

    // Parses a NOMADS `.idx` text file for the byte range of the record
    // matching `match` (a substring like ":TMP:2 m above ground:"),
    // resolving to the record ending exactly on `stepHour` when a field has
    // multiple sub-hourly-step records (0 = take the first match, the
    // common case for a synoptic-only or top-of-hour file). Public (was
    // private) so sibling RRFS-derived products doing their own byte-range
    // fetch against a differently-shaped URL (UtilityRefs, against
    // `refs.*`/`rrfsens.*` rather than `rrfs.*`) can reuse the parser
    // instead of a second copy.
    static bool idxByteRange(const string& idxText, const string& match, long long& start, long long& end, int stepHour = 0);

private:
    // shared cache-or-generate wrapper - every *LinesGeoJson() accessor
    // above (except cwaLinesGeoJson(), which extracts an already-geojson
    // resource rather than parsing a *.bin segment file) is a one-liner
    // over this.
    static string cachedLinesGeoJson(const string& cacheFileName, const string& resourceBinName);
    static bool resolveLatestRun(string& dateStr, string& cycle);
    static void purgeCacheForOldRun(const string& keepRunId);
    static string cacheDir();
};

#endif  // UTILITYGRIB_H
