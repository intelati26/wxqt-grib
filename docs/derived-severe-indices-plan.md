# Derived severe-weather indices (SHIP, SIGTOR/STP) — plan

Status: **planning**, 2026-09-12. Continues a conversation the user had
elsewhere that produced a working Python/MetPy/xarray prototype for **SHIP**
(Significant Hail Parameter) computed from RRFS fields, dual-contoured with
native HAILCAST max hail diameter. Goal: port the computation into wxqt as a
new derived product, the same way "2m Temp / Dewpoint" and "2m Temp / Dew /
Wind" are already derived products in `UtilityGrib` - but SHIP needs ~9 input
fields and real thermodynamic math, not 2 fields and a straight fill+contour,
so it needs its own small piece of architecture, not just a new `Field` row.

## SHIP formula (confirmed against the Python prototype + SPC's mesoanalysis definition)

```
SHIP = (MUCAPE * MIXR * LR75 * (-T500) * SHEAR6) / 42,000,000
```

Before multiplying, three inputs are capped:
- `SHEAR6` (0-6 km bulk shear) clamped to **[7, 27] m/s**
- `MIXR` (MU-parcel mixing ratio) clamped to **[11, 13.6] g/kg**
- `T500` capped at **-5.5 C** (any warmer value -> -5.5; colder untouched)

After multiplying, three conditional rescalers:
- `MUCAPE < 1300 J/kg` -> `SHIP *= MUCAPE/1300`
- `LR75 < 5.8 C/km` -> `SHIP *= LR75/5.8`
- `freezing level < 2400 m AGL` -> `SHIP *= FZL_AGL/2400`

Interpretation (per SPC, carried over from the prototype's docstring): SHIP
is *not* a forecast hail size. >1.00 = environment favorable for significant
(>=2") hail; >4 = very high; SIG hail reports typically coincide with max
contour values of 1.5-2.0+. MUCAPE dominates the product enough that one bad
surface ob can produce a spurious CAPE bullseye and an equally spurious SHIP
spike - worth a gut-check against the raw CAPE field before trusting an
isolated hotspot (worth surfacing as in-app help text, not just a code comment).

Layer 2 (line contours): native `HAIL:surface:N-N hour max fcst` straight out
of `2dfld`, meters -> inches, no computation - this part is a **drop-in fit**
for the contour mechanism `UtilityGrib` already has for height fields.

## Field mapping — verified against a live RRFS run (2026-09-12, F003, 00z)

This corrects/simplifies the prototype's `FIELD_MAP`, which was written from
best-guess UPP conventions with a "verify before running" warning baked in.
Verified via `.idx` grep against a real file - **everything SHIP needs is in
`2dfld` except T700/T500**, and 0-6 km shear is a *native* field, not
something to interpolate from isobaric U/V (the prototype hoped for this but
wasn't sure it'd be there):

| Prototype's guess | **Confirmed real field** | File |
|---|---|---|
| `hail` / surface / shortName hail | `HAIL:surface:N-N hour max fcst` | 2dfld |
| `mucape` / pressureFromGroundLayer 180-0mb | `CAPE:180-0 mb above ground` | 2dfld |
| `t2`, `d2` | `TMP:2 m above ground`, `DPT:2 m above ground` | 2dfld |
| (RH derived from t2/d2 via Magnus) | **`RH:2 m above ground` ships natively - skip the Magnus-formula derivation entirely** | 2dfld |
| `sp` | `PRES:surface` | 2dfld |
| `t700`, `t500` | `TMP:700 mb`, `TMP:500 mb` | prslev |
| `ushr6`/`vshr6` guessed as `heightAboveGroundLayer` vucsh/vvcsh, "check first" | **Confirmed present**: `VUCSH:0-6000 m above ground`, `VVCSH:0-6000 m above ground` - **in `2dfld`, not `prslev`** | 2dfld |
| `fzl_msl` guessed `isothermZero`/`gh`, "verify - naming drifts" | **`HGT:0C isotherm`** | 2dfld |
| `terrain` | `HGT:surface` | 2dfld |

Bonus fields confirmed present in the same `2dfld` file, useful for STP later
(see below): `CAPE:surface`/`CIN:surface` (SBCAPE/SBCIN), `HLCY:1000-0 m
above ground` (0-1 km SRH - the standard STP input), `HLCY:3000-0 m above
ground` (0-3 km SRH), `CAPE:0-3000 m above ground` (0-3 km CAPE), `DCAPE:400-0
mb above ground`. `prslev` also carries `HGT:700 mb`/`HGT:500 mb` alongside
the temperatures, which means the 700-500 mb lapse rate can be computed as a
**direct height difference** (`LR75 = (T700-T500) / ((HGT500-HGT700)/1000)`)
instead of the prototype's hypsometric-equation approximation from mean
temperature - simpler and more accurate once both HGTs are already being
fetched anyway. Net result: **10 grib records** (8 from `2dfld`, 2 from
`prslev`) instead of the dozen-ish the prototype's field map implied.

## Architecture: this doesn't fit the `Field` table - it's its own render path

`UtilityGrib::Field` (used by `render()`) is built for "one fill field,
optionally one contour field." SHIP is nine-ish inputs feeding real formula
math. wxqt also has no MetPy/numpy/xarray - everything is GDAL-CLI shell-outs
(`gdalwarp`, `gdaldem`, `gdal_calc`, `gdal_contour`). The translation target
is a **chain of `gdal_calc` invocations**, which turns out to work well:
`gdal_calc`'s `--calc` expression runs in a numpy-backed evaluator - already
confirmed this session that it exposes at least `minimum()`; `numpy.clip`
and `numpy.where`-equivalent (`where()`) are standard in that same evaluator
and should cover every capping/rescaling step in the formula without needing
custom code per step.

Proposed shape, a new sibling to `UtilityGrib`/`UtilitySpcPost` (not an
extension of either - it shares their gdalwarp/cache-dir/byte-range-fetch
patterns but its `render()` doesn't fit either one's shape):

- **`src/models/UtilitySevereIndices.{h,cpp}`** (name tentative - open to
  `UtilitySpcProxy`, `UtilityDerivedIndices`, etc.)
  - Reuses `UtilityGrib::fetchFieldSlice`-style byte-range fetch (make that
    method public, or duplicate the ~30 lines - it's already been duplicated
    once between `render()`'s local lambda and `fetchFieldSlice`, so a third
    copy needs a better answer: probably make `fetchFieldSlice` public on
    `UtilityGrib` rather than triplicate it).
  - `render(int indexIndex, int regionIndex, forecastHour, runId, status)` -
    for `indexIndex == 0` (SHIP, the only one specified so far):
    1. Fetch the 10 records above (one `fetchFieldSlice` call each).
    2. `gdalwarp` each to the same bbox/resolution (mirrors the main
       pipeline's `-dstnodata -9999` warp).
    3. `gdal_calc` steps: mixing ratio from T2+RH2+surface pressure (Bolton
       saturation-vapor-pressure formula, one expression); LR75 from the two
       HGT/TMP pairs; shear6 magnitude (`sqrt(A**2+B**2)`) from
       VUCSH/VVCSH; FZL_AGL (`HGT:0C isotherm - HGT:surface`).
    4. One final `gdal_calc` with up to 6 named inputs (`-A` through `-F`)
       encoding the *entire* cap + multiply + conditional-rescale formula in
       a single `--calc` expression (this is the piece to prototype/test
       first - if `where()`/`clip()` aren't available in this gdal_calc
       build, cascading `gdal_calc` steps per rescaler is the fallback, just
       more processes).
    5. `gdaldem color-relief` with a SHIP-specific table built around the
       prototype's own contour levels (0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0 -
       these already read as sensible color-relief breakpoints).
    6. Hail contour: warp `HAIL:surface` -> convert m to inches
       (`gdal_calc --calc="A*39.3701"`) -> `gdal_contour` at inch levels
       (0.75, 1.0, 1.5, 2.0, 2.5) -> burn onto the SHIP fill, reusing the
       *exact* contour-burn-and-label mechanism `UtilityGrib.cpp` already has
       for height fields (`labelContours()`, the `ST_Buffer`+
       `ST_SimplifyPreserveTopology` smoothing step) - no new code needed
       there, just a new caller.
    7. Apply the nodata-transparency fix from [[wxqt-grib-viewer]] /
       [[wxqt-spcpost-rrfs-background]] from the start this time, not as a
       later patch - build the mask+merge into this pipeline on day one.
  - A small `struct Index { label; key; ... }` table (mirrors `Field`) so
    adding SIGTOR later is "append a row" once its formula/fields are
    equally verified, not a new code path.
- **New `IndexViewer` window**, or fold into `GribViewer` as another combo
  option alongside the field picker? Leaning **new small viewer** (mirrors
  `SpcPostViewer`'s relationship to `GribViewer` - related but a different
  enough pipeline that forcing it into one class would tangle both). Reuses
  `AnimationBar`, `ZoomImage` (+ hover via the shared `SampleGrid`), `UtilityJxl`
  save - all already generic. Toolbar entry near the other RRFS-derived
  buttons.
  - **Hover read-out is a natural fit here** - the whole point of a
    disclaimer like "gut-check against raw MUCAPE" is easiest to act on if
    hovering shows you the SHIP value at a point; a stretch goal is showing
    *several* of the intermediate grids (MUCAPE, MIXR, LR75, shear6) in one
    hover tooltip, not just the final SHIP number, directly enabling that
    gut-check instead of requiring a separate MUCAPE-only view.
  - **This is meant to be a "Parametric" viewer, not a "SHIP" viewer** (user,
    2026-09-12) - the `Index` table is the point: SHIP first, STP next,
    others later, all through one combo, not a one-off screen. Frame the UI
    around "Parametric" rather than naming SHIP anywhere in chrome that a
    second index would make stale.
  - **Eventual 24hr-max mode** (user, 2026-09-12): the user's actual target
    isn't a single-hour SHIP snapshot, it's a **24hr max SHIP** swath -
    render SHIP at each hour across a window (e.g. F001-F024) and take the
    pixel-wise max, the parametric-ingredients analog of the existing SPC
    Post 24hr *probability* product and the human-issued SPC Day 1 outlook.
    The user's stated vision is these three side by side as different lenses
    on the same threat: **SPC forecast** (human outlook, already in the app
    via `SpcSwoDay1`/`SpcSwoSummary`), **SPC Post 24hr** (ensemble
    probability, already working - see [[wxqt-spcpost-viewer]]), and
    **Parametric 24hr max** (raw-ingredients favorability, this feature).
    Mechanically this is a pixel-wise max fold across N single-hour SHIP
    tifs (`gdal_calc -A f01 -B f02 --calc="maximum(A,B)"`, chained, or a
    `gdal_merge -separate` stack reduced some other way) - deferred until
    Segment 3's single-hour formula exists and is trusted, but the viewer
    shell (Segment 5) should be designed with an hour-range control from the
    start rather than bolting one on after.

## SIGTOR / STP

**Resolved 2026-09-14** - user confirmed "standard, or the most cutting edge
if available." True effective-layer STP (SPC's current default form) needs
ESRH/EBWD/effective-inflow-base, which require a full vertical-profile
parcel test RRFS doesn't ship as ready fields - substantially bigger scope
than SHIP was, so **deferred** (user: "save the effective layer" for later).
Built instead: **STP (fixed layer, Thompson et al. 2003)**, SPC's own
documented predecessor form
(spc.noaa.gov/exper/mesoanalysis/help/help_stor.html):

```
STP = (SBCAPE/1500) x LCL-term x (SRH1/150) x shear-term x CIN-term
```
- `LCL-term` = `(2000-sbLCL)/1000`, floored to 1.0 below 1000 m AGL, 0.0 above 2000 m
- `shear-term` = `6BWD/20`, capped at 1.5 above 30 m/s, 0.0 below 12.5 m/s
- `CIN-term` = `(200+sbCIN)/150`, floored to 1.0 above -50 J/kg, 0.0 below -200 J/kg

Every input turned out to be a direct RRFS field, same shape as SHIP -
**correcting an earlier note in this doc**: LCL height is NOT missing, it's
`HGT:level of adiabatic condensation from sfc` in the same `2dfld` file
(MSL, like every other `HGT:` field here - subtracted from `HGT:surface`
for AGL, the same convention SHIP's own freezing-level term already uses).
0-6km shear reuses SHIP's own `VUCSH`/`VVCSH:0-6000 m above ground` fields
directly (`UtilitySevereIndices::shipInputFields[5]`/`[6]`), not
redeclared. New fields (`UtilitySevereIndices::stpInputFields`): `CAPE:surface`,
`CIN:surface`, `HLCY:1000-0 m above ground`, `HGT:level of adiabatic
condensation from sfc`.

Implemented as `UtilitySevereIndices::computeStpGrid()` + generalized
`render()`/`IndexViewer` to a real second `Index` row (`indices[1]`, key
`"stp"`) - see `wxqt-severe-indices-plan` memory for the render()
generalization (both grids now flow through one shared
color/mask/composite/hover-grid pipeline, SHIP's hail-contour overlay the
only index-specific tail). Verified live 2026-09-14 against a real RRFS run
(`2026091406`, F004, CONUS): rendered PNG valid RGBA with correct nodata
alpha, dataMin/dataMax -0.286..1.167 (physically sane for a weak-shear
morning environment), hover `.grid` sidecar present. Field-fetch batching
(`fetchFieldSlices`) picked STP's 7 inputs up for free through
`fetchAndWarpAll` - 3 byte-range GETs instead of 7.

## Staged milestones (targeted segments, per the user's standing instruction)

- **Segment 1 - DONE (2026-09-12)** — `fetchFieldSlice` made public on
  `UtilityGrib`. New `src/models/UtilitySevereIndices.{h,cpp}` stands up a
  13-field debug fetch (`debugFetchShipInputs`): byte-range fetch each field
  via the now-public `fetchFieldSlice`, `gdalwarp` to a common CONUS grid,
  `gdalinfo -mm` for the value range, `gdal_translate -scale` to an
  auto-stretched grayscale PNG. Verified against a real run
  (`2026091200`, f003): all 13 fields fetched, every value range physically
  sane (MUCAPE 0-4238 J/kg, T500 -21.6..-1.5 C, hail 0-0.047 m, etc - full
  table in session notes), and two fields eyeballed directly - terrain shows
  an unmistakable Rockies/Appalachians CONUS silhouette, MUCAPE shows a
  coherent convective plume/axis (not noise).

  **Two real findings from this segment, both feed Segment 2/3:**
  1. **RRFS's full 2dfld/prslev field set is synoptic-cycle-only
     (00/06/12/18z).** An off-cycle hourly run's subh file (e.g. 02z, 156
     records vs 318 in the 00z plain file) is missing HAIL, CAPE:180-0mb,
     VUCSH/VVCSH, and HGT:0C isotherm entirely - confirmed by direct `.idx`
     diff. **SHIP can only be rendered for 00/06/12/18z runs**, not every
     hourly RRFS cycle. `IndexViewer`'s run/hour picker needs to reflect
     this (offer only synoptic runs, or show a clear "not available this
     hour" rather than a silent failure).
  2. **Surface pressure (`PRES:surface`) comes through in native Pa, not
     hPa** (observed range 65466-101915 Pa) - unlike temperature, GDAL's
     GRIB unit normalization does not touch pressure. The mixing-ratio
     Bolton-formula step in Segment 2 needs `sp/100` before use.

  Test harness note for future segments: the standalone relink-test pattern
  ([[wxqt-severe-indices-plan]]) needs a `QCoreApplication` instance in
  `main()` - without one, `URL::getText`/`getBytesRange`'s `QEventLoop::exec()`
  never gets a working event dispatcher and the request hangs forever with
  no error (cost 25 real minutes the first time before this was diagnosed).
  Every future standalone test against this codebase's networking should
  start from `QCoreApplication app{argc, argv};`. **Update (Segment 4):** if
  the code path under test also draws anything via `QPainter` (contour
  labels, station plots, etc.), use `QGuiApplication` instead -
  `QCoreApplication` never loads the Qt platform plugin at all, so
  `QT_QPA_PLATFORM=offscreen` does nothing for it and any `QPainter::drawText`
  call fatals in the font database. `QGuiApplication` under
  `QT_QPA_PLATFORM=offscreen` handles both networking and text/image drawing
  correctly with no display.
- **Segment 2 - DONE (2026-09-12)** — added `fetchAndWarpAll()` (fetches +
  warps a caller-supplied subset of `shipInputFields` to a common CONUS
  grid; `debugFetchShipInputs` from Segment 1 now goes through it too) and
  `debugComputeDerivedGrids()`, which chains four `gdal_calc` steps and
  dumps each as an auto-scaled grayscale PNG. Verified against the same
  `2026091200` f003 run - all four sane and internally consistent with
  Segment 1's numbers:
  - **mixr** (g/kg): Bolton saturation-vapor-pressure-at-Td formula,
    `622*e/((sp/100)-e)` with `e=6.112*exp(17.67*Td/(Td+243.5))` - confirms
    `exp()` works in this gdal_calc build (see below). Range 1.82-25.32
    g/kg; PNG eyeballed and shows exactly the expected moisture gradient -
    dry Intermountain West, moist Southeast/Gulf.
  - **lr75** (C/km): direct height difference,
    `(T700-T500)/((HGT500-HGT700)/1000)`, no hypsometric approximation
    needed. Range 4.2-9.7 C/km (dry-adiabatic ceiling is 9.8, so the upper
    end is physically right at the limit, as expected over arid terrain).
  - **shear6** (m/s): `sqrt(ushr6^2+vshr6^2)`. Range 0.014-46.49 m/s,
    consistent with Segment 1's raw component ranges.
  - **fzlagl** (m): `HGT:0C isotherm - HGT:surface`. Range 313.48-5602.125
    m - the max matches Segment 1's raw `fzlmsl` max *exactly*, i.e. the
    pixel where terrain=0 (sea level) passed through unchanged - a good
    internal-consistency signal that the height-difference math is right.

  **Confirms Segment 3's biggest unknown is resolved**: read
  `osgeo_utils/gdal_calc.py`'s source directly (not just tested one
  function) - its eval namespace is built from every public attribute of
  `numpy` (`{key: getattr(module, key) for module in [gdal_array, numpy]
  for key in dir(module) ...}`), so `exp`, `sqrt`, `where`, `clip`,
  `minimum`, `maximum` are all available by name, not just the `minimum()`
  confirmed earlier. The full cap+multiply+rescale SHIP formula can very
  likely be one `--calc` expression using `clip()`/`where()`, as hoped -
  Segment 3 should still verify the exact syntax against this installed
  version, but there is no longer a real doubt it's *possible*.
- **Segment 3 - DONE (2026-09-12)** — added `debugComputeShip()`: computes
  the three intermediate grids (mixr, lr75, shear6 - fzlagl folded inline
  as `F-G` in the final expression rather than its own tif), then the
  entire cap+multiply+rescale formula as **one** `gdal_calc` expression:
  ```
  (A*clip(B,11.0,13.6)*C*(-minimum(D,-5.5))*clip(E,7.0,27.0)/42000000.0)
  *where(A<1300.0,A/1300.0,1.0)*where(C<5.8,C/5.8,1.0)*where((F-G)<2400.0,(F-G)/2400.0,1.0)
  ```
  (A=MUCAPE, B=MIXR, C=LR75, D=T500, E=SHEAR6, F=FZL MSL, G=terrain).
  Confirms the Segment 2 finding held up under a real multi-clause
  expression, not just individual functions in isolation. Added
  `shipColorMap` (breakpoints 0/0.5/1.0/1.5/2.0/3.0/4.0/6.0, per SHIP's own
  interpretation thresholds) and render via plain `gdaldem color-relief`
  (no alpha/nodata-mask treatment yet - that's explicitly deferred to the
  production pipeline in Segment 5, per the note above about doing it right
  from the start there).

  Verified against the same `2026091200` f003 run: SHIP range 0.00-1.69
  across CONUS. **Cross-checked against an independent product**: the SPC
  Post 24hr calibrated hail probability for essentially the same period
  (checked in the same session) showed only two small, low (10-20%)
  probability blobs - a quiet-but-not-dead severe day. SHIP's modest max of
  1.69 (favorable-for-hail starts at 1.0, "very high" is >4) is consistent
  with that independently-sourced read, which is a meaningfully stronger
  confirmation than "the raster isn't garbage" - two differently-derived
  products (RRFS-parametric vs HREF/GEFS-ensemble-probability) agree on the
  day's severity level. The colorized PNG also shows a single coherent
  favorable axis in roughly the same Plains/Gulf-moisture region as
  Segment 1's raw MUCAPE plume, not scattered noise.
- **Segment 4 - DONE (2026-09-12)** — `debugComputeShip()` extended:
  color-relief now renders to a GeoTIFF (not straight to PNG) so contour
  lines can be burned in with correct georeferencing, then HAILCAST is
  fetched, warped, converted m->inches, contoured at fixed levels
  (0.75/1.0/1.5/2.0/2.5"), smoothed (`ST_SimplifyPreserveTopology` +
  `ST_Buffer`, same recipe as `UtilityGrib`'s height-field contours), burned
  onto the colored SHIP tif, and labelled - via a new `labelHailContours()`
  in the anonymous namespace (a trimmed copy of `UtilityGrib.cpp`'s
  `labelContours()`, which is unreachable from here since it lives in that
  file's own anonymous namespace - consistent with this project's existing
  per-class small-helper duplication, e.g. `gdalBinDir()`/`cacheDir()`).

  **Two real bugs found and fixed while verifying, not just "looks right":**
  1. **The coarse-warp resolution UtilityGrib uses for height-field contours
     (cubicspline, 190 columns) silently killed every HAILCAST contour.**
     First test (F003, HAIL max 1.85") produced only a single ambiguous
     white pixel; a second test at F021 (HAIL max 2.56", scanned across
     several forecast hours specifically to get a bigger signal to check
     against) produced **zero** contour features - `gdal_contour` wasn't
     broken, the smoothed input legitimately no longer had any value above
     the lowest 0.75" breakpoint (coarse warp brought the true 2.56" peak
     down to 0.59"). Root cause: HAILCAST is a spiky, storm-scale field:
     smoothing tuned for synoptic height fields destroys it. Confirmed via
     a manual step-by-step shell reproduction (kept every intermediate
     file) before touching the C++. Fix: `-r near -ts 800` (nearest-
     neighbor at the main fill's own resolution, preserving peaks) instead
     of `-r cubicspline -ts 190` for this one warp - went from 0 to 165
     contour features on the same input. Also fixed the silent
     `gdal_rasterize` call (result wasn't checked - exactly the kind of gap
     that let this hide) to only mark contours as labelled if the burn
     actually reported success.
  2. **The standalone relink-test harness needs `QGuiApplication`, not
     `QCoreApplication`, once the code path draws anything via `QPainter`.**
     `QCoreApplication` never loads the Qt platform plugin at all, so
     `QT_QPA_PLATFORM=offscreen` was a no-op and the font database was
     never initialized; `QPainter::drawText` (inside the new
     `labelHailContours`) hit `QFontDatabasePrivate::findFont` and fataled.
     The F003 test happened not to crash only because its one contour
     fragment was too short to pass the noise-length filter, so `drawText`
     was never actually called there - a false negative that could have
     hidden this indefinitely. Confirmed via `gdb -batch -ex run -ex bt`.
     Production code is unaffected (the real app runs under `QApplication`,
     which does initialize the platform/font backend) - this is purely a
     test-harness requirement, now added to the harness pattern note below.

  Re-verified after both fixes at F021: 165 real contour features, visible
  white rings scattered across CONUS (matches HAILCAST's actual character -
  many small discrete storm-scale hail cores rather than one blob, which is
  physically correct for a convection-allowing model), green-text/white-halo
  "0.75"" labels placed near several cores, no crash.

  **Follow-up (user, 2026-09-12): "work upstream, don't hide changes from
  the rest of the program."** The first version of this labelled the hail
  contours with a `labelHailContours()` duplicated into
  `UtilitySevereIndices.cpp` because the real `labelContours()` lived in
  `UtilityGrib.cpp`'s anonymous namespace, unreachable from here - the same
  situation `fetchFieldSlice` was already in before this session made it
  public. Fixed the same way: moved `labelContours` out of that anonymous
  namespace into `UtilityGrib`'s public API (`UtilityGrib.h`/`.cpp`), added
  `decimals`/`suffix` parameters so it can format either a rounded height in
  gpm (`render()`'s own call, defaults `0`/`""` preserve exact prior
  behavior) or a hail diameter in inches (`2`/`"\""`), and deleted the
  duplicate. Re-verified after the refactor: identical result (SHIP max
  1.268, "with hail contours"), confirming the move was behavior-preserving.
  `drawBarb()` and whatever follows it in that file's anonymous namespace
  were untouched (that namespace still exists, just no longer wraps
  `labelContours`).
- **Segment 5a - DONE (2026-09-13)**: `UtilitySevereIndices::render()` -
  the production API `IndexViewer` will call. Cached (`si1_` prefix),
  region-aware (any of `UtilityGrib::regions()`, not just CONUS), builds a
  `.grid` hover sidecar, and applies the same nodata-transparency treatment
  as `UtilityGrib`/`UtilitySpcPost`'s main pipelines. `debugComputeShip()`
  and `render()` now share one `computeShipGrid()` helper for the actual
  formula math - it existing separately from the debug path was the whole
  point, not a fork of it. Also: `indexLabels()` (currently just SHIP -
  `UtilitySevereIndices` is deliberately shaped as a "Parametric" combo, not
  a SHIP-only class) and `regionLabels()` (a thin pass-through to
  `UtilityGrib::regions()`, not a duplicated table).

  **Real bugs found and fixed while wiring this up, in the order hit:**
  1. **Upstream duplication fix, before it could triple**: after the
     `labelContours` lesson ([[wxqt-code-style]]), the render-resolution
     tiering (`mainRenderColumns`) had *already* been re-duplicated into
     `UtilitySpcPost.cpp` instead of shared. Adding a third copy for
     `UtilitySevereIndices` would have made that worse, so fixed all three
     at once: `nativeGridColumns`/`mainRenderColumns`/`regionBbox` moved to
     `UtilityGrib`'s public API, `UtilitySpcPost`'s copy now delegates to it.
  2. **NOMADS rate-limit self-pacing, centralized** (user asked directly:
     "is there a simple way for the program to count calls and self-wait?")
     - rather than each call site inventing its own sleep (this codebase had
     two different ad-hoc ones already), added a single per-host throttle
     inside `URL.cpp`'s four entry points (`getText`/`getTextXmlAcceptHeader`/
     `getBytes`/`getBytesRange`): each host gets a reserved "next slot" under
     a mutex (safe against the app's real parallel-fetch paths -
     `DownloadParallelBytes`, `QtConcurrent`-backed `FutureBytes`/`FutureVoid`),
     minimum 300ms apart, sleep happens outside the lock so unrelated hosts
     are never held up. This is now every network call's problem to not
     have, not each caller's problem to solve.
  3. **A real, previously-latent bug in `UtilityGrib::fetchFieldSlice`
     itself**: its cache-validity check only verified the first 4 bytes
     ("GRIB") - a byte-range fetch truncated by a mid-transfer network
     hiccup can pass that check while missing GRIB2's mandatory 4-byte
     "7777" end-of-message marker, and once cached, it was *never*
     re-validated, silently breaking every future gdalwarp that touched it.
     Found live: a truncated cached "sp" record poisoned every SHIP render
     for that run/hour. Fixed by also checking the trailing 4 bytes on
     cache-hit, deleting and re-fetching if either end is wrong. (The
     specific run this surfaced on, `2026091200`, turned out to have also
     genuinely rotted server-side by the time it was retested a day later -
     NOMADS `para` retention rolling an old day out - so testing moved to
     the then-current run; that's an external data-lifetime fact, not a
     wxqt bug, but worth remembering: **don't hardcode a specific historical
     run in a test that might outlive it**.)
  4. **The actual masking bug, the subtle one**: the nodata mask was first
     built by checking `shipTifPath` (the SHIP formula's own output) for
     `==-9999`. `gdal_calc`'s `--calc` does not propagate NoData through
     arithmetic - an input of `-9999` to `A*clip(B,...)*...` just produces
     some other arbitrary number, not `-9999`. So the mask never matched
     anything, and every off-domain corner rendered fully opaque (confirmed
     wrong by direct pixel check - a plain image viewer can't tell "opaque
     white" from "transparent over a white pane", the same trap noted for
     SpcPostViewer's own nodata fix). Fixed by threading a `nodataRefPath`
     out of `computeShipGrid()` - one raw input warp (`mucape`'s, chosen
     arbitrarily since all inputs share the same footprint), kept
     un-deleted specifically so callers needing a nodata mask build it from
     an *unprocessed* field, not the formula's output. Verified: CONUS
     corner now alpha=0 (transparent), in-domain pixels still alpha=255,
     hail contours and SHIP colors unaffected.

  Verified end-to-end against a fresh run: CONUS (867x385, matching the
  half-native tiering) and Southeast (420x293, matching 1:1 native) both
  render correctly with transparent edges, hail contours, working `.grid`
  hover sidecars, human-readable status lines, and instant cache hits on a
  second call.

- **Segment 5b - DONE (2026-09-14)**: `src/models/IndexViewer.{h,cpp}` -
  deliberately mirrors `GribViewer` almost line-for-line rather than
  `SpcPostViewer`, since `UtilitySevereIndices::render()`'s signature shape
  (index/region/forecastHour/runId, all locally resolvable) matches
  `UtilityGrib::render()` exactly - no server-directory-scraping the way
  SpcPostViewer's product/run discovery needs. Same `AnimationBar`-driven
  sweep, `ZoomImage`+`SampleGrid` hover, `UtilityJxl` save. Toolbar entry
  added (`tstorm.png` icon - no dedicated hail icon exists in the bundled
  resources, this was the closest generic-severe-weather one available).

  Two real gaps closed while building this, both upstream to
  `UtilitySevereIndices` rather than UI-only:
  1. **"Latest" would have silently broken almost every render.**
     `UtilitySevereIndices::render()` used to resolve an empty `runId` via
     `UtilityGrib::getLatestRun()` - the latest *hourly* run, not the latest
     *synoptic* one Segment 1 already proved SHIP needs. New
     `resolveSynopticRun()` (used by both `computeShipGrid()`'s fetch path
     and `render()`'s own cache-key resolution) filters
     `UtilityGrib::runOptions()` down to 00/06/12/18z cycles first, falling
     back to `getLatestRun()` only if literally no synoptic run is found.
     New public `UtilitySevereIndices::runOptions()` (same filter) and
     `forecastHours()` (thin pass-through) give `IndexViewer`'s combos the
     same synoptic-only picture, so nothing in the UI can even select a run
     that would fail.
  2. **The "Parametric" combo needed to be a real table, not a label list**,
     once `IndexViewer::buildLegend()` needed an actual color-relief table
     to draw from (`GribViewer`'s legend reads `UtilityGrib::fields[i]
     .colorMap`) - `indexLabels()` alone couldn't supply that. Added
     `UtilitySevereIndices::IndexEntry {label, key, units, colorMap}` and a
     public `indices` table (one row today, SHIP; `indexLabels()` now
     derives from it) - this is also exactly the shape Segment 6 needs to
     append SIGTOR/STP as a second row.

  Verification limits, stated plainly: this Wayland session can't do GUI
  click-automation (no xdotool), so the actual on-screen interaction wasn't
  click-tested - confirmed the app starts without crashing/erroring, but
  that's a much weaker signal than the extensive live backend testing
  Segment 5a already got. Confidence here rests on (a) the backend being
  proven independently and (b) the UI code being a close structural mirror
  of `GribViewer`, an already-shipped feature using the identical pattern -
  not on having watched it render on screen.
- **Segment 6 - DONE (2026-09-14)** — STP (fixed layer) as a second `Index`
  table row, reusing everything segments 1-5 built: `fetchAndWarpAll`
  (including its `fetchFieldSlices` batching for free), the color/mask/
  composite/hover-grid pipeline in `render()` (generalized to branch on
  `indexIndex` rather than forking), `IndexViewer`'s combo/legend/save
  plumbing (already index-agnostic except one hardcoded hover-label string,
  fixed). True effective-layer STP explicitly deferred - see "SIGTOR / STP"
  above.
