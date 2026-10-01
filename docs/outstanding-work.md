# Outstanding work — index

Short pointer doc, not a plan in itself. Each item links to its own detailed
plan doc or memory where one exists. Updated 2026-09-14.

## Windows packaging - moved out of this repo (2026-09-14)
The Windows portable-folder packaging script (`package_windows.bash`) and
its scoping notes now live in a **separate project**,
`../wxqt-windows-portable/`, deliberately kept out of this repo (this
project is headed to a public GitHub/GitLab remote; packaging tooling and
its research notes don't need to travel with the app's own source history).
`README_WINDOWS.md` here stays as upstream had it - no pointer back to the
other project, by design.

One real code bug that scoping pass found is still open, in *this* repo,
independent of packaging: `gdal_calc`/`gdal_merge`'s compiled-vs-`.py`
fallback check (`QFile::exists(bin + "gdal_calc")`, used in
`UtilityGrib.cpp`/`UtilitySpcPost.cpp`/`UtilitySevereIndices.cpp`/
`UtilityRefs.cpp`) doesn't handle Windows' `.exe` extension and wouldn't
correctly invoke a bare `.py` file there either (no shebang support) - on
Windows this needs to become "does `gdal_calc.exe` exist → run directly;
else run `python.exe gdal_calc.py <args>` explicitly." Not yet fixed - low
priority until Windows packaging is actually attempted for real.
(The `QSettings`-defaults-to-the-registry finding from that same pass **is**
fixed now, see "Recently closed" below - unrelated to Windows specifically,
worth doing before any public push regardless of platform.)

## Lite build / package size (2026-09-30, local)
Default build no longer includes QtWebEngine (Chromium was most of the
download and only the Observation Sites screen used it). `./makeAll.py
--webengine` restores it (defines `WXQT_WEBENGINE`); the lite build's preview
panes show a visible note + link instead of a blank box. CI prints a package
size report (top-level entries + 25 largest files) in each job log - use it
to see what is left. **Future:** render the observation data as a native
widget like the other screens, then the web engine can go away for good.
Windows now bundles a minimal vcpkg GDAL (`.github/vcpkg/vcpkg.json`: core
drivers + GEOS/PNG/SQLite/OpenJPEG) instead of MSYS2's full one, whose closure
(OpenBLAS, Arrow/Parquet, x265/aom/SVT-AV1, SFCGAL, poppler, HDF5, ...) was
most of the 147 MB zip. Contour buffering no longer needs SpatiaLite
(`UtilityGrib::bufferContours`: `ogr2ogr -simplify` then `ST_Buffer`, output
byte-identical). CI smoke-tests every GDAL tool/driver the app uses.
**Result:** Windows zip 275 MB -> 47 MB over this work (lite build, packaging
trims, minimal GDAL); AppImage 171 MB -> 87 MB. AppImage is already
zstd-compressed (checked in the appimagetool log), so xz is not a free win.
**Linux minimal GDAL - DONE (PR #2):** the AppImage now bundles the same vcpkg
minimal GDAL (shared libs, release-only triplet overlay in
`.github/vcpkg/triplets`), stripped, instead of Ubuntu's gdal-bin:
**87 MB -> 66 MB** (171 MB at the start of this work). New CI checks run the
bundled GDAL from a clean environment (`.github/vcpkg/smoke-linux.sh`) and
require the finished AppImage to start headless and stay up 12 s. Remaining
weight is Qt + ICU data (~29 MB) + libgdal; the packaging cannot be run locally
(CachyOS IFUNC issue), so CI is the test.

## Model map rendering (2026-10-01)
Found by measurement: meso sectors were sampled at 3.0 km/px horizontally but
3.7-4.2 km/px vertically (equal-degree raster sized by width only). Now: SPC-meso /
"My Area" maps render at **2 pixels per 3 km cell in each direction**, sized by the
finer (latitude) axis, resampled with **cubic** (smooth colours kept on purpose -
no banded option wanted), i.e. ~980x683 for a sector (was 381x266); CONUS stays at
half native. Reference: SPC's own HRRR GIFs are 1000x750 for every sector, ~1.6 km/px
(= roughly this 2x2), banded fills, plain 1-px lines. State/county/CWA lines are
now drawn **anti-aliased** at the image's own resolution (`UtilityGrib::drawMapLines`,
widths scale with image size) instead of burned in as 1-px raster lines; used by the
GRIB, REFS and SPC Post renders. The SPC-style **information bars** (model, product,
units, region / run, F-hour, valid UTC + local) are added only on **export**
(`UtilityAnimationExport::withHeader`, per frame for loops, read from each frame's
status line), so the on-screen map stays clean and hover geometry is untouched.
Cache versions bumped (r11, rm4, rf4/pb4/pm2, sp6, si3/sm2).

## Home screen rows (2026-10-01)
The home screen's image / forecast / text sections are now **rows stacked top to
bottom** (default order: images, forecast, text) right of the toolbar, instead of
columns. Same Settings > Home Screen Order control, relabelled "Rows". Each section
layout just flips to left-to-right (`MainWindow::MainWindow`, `arrangeColumns`);
the forecast sub-layouts are top-aligned so current conditions line up with the
7-day list. Not verified by eye by this session. Possible follow-ups: lay the 7-day
days out horizontally (that row is tall now), wrap the image row when many
thumbnails are enabled (it widens the window instead).

## Soundings, saved views, window memory (2026-10-01)
- **One sounding path:** the toolbar Soundings screen and the radar-site sounding shortcut open the native
  `SoundingViewer` in observed mode (site + time pickers inside it). `SpcSoundings` (the SPC GIF screen with a
  Text Display button and site stepping) is left in the code, unused. The home screen has an optional
  "SPC Sounding (nearest site)" thumbnail (SPC's GIF; click opens the native viewer).
- **CI:** the `sounding-engine` job is gone from `build.yml`; the regression test still exists in `tests/sounding/`.
- **Duplicate formulas:** STP / SHIP exist in `UtilitySevereIndices` (gridded, from published fields) and
  `SoundingIndices` (point, from a full profile). Not merged; SHIP differs from SPC on 25/160 observed soundings.
- **GRIB viewer saved views:** Saved views dropdown + Save/Delete view (field, region, compare mode; pref
  `GRIB_SAVED_VIEWS`). Not yet in the index / SPC Post / REFS viewers.
- **Window memory:** every `Window` saves its geometry on close and restores it on first show, per screen type
  (`Window::showEvent`, pref `WINDOW_GEOMETRY_<type>`). No settings toggle yet.

## Home layout editor, toolbar styles, middle-drag (2026-10-01)
- **Home layout editor:** Settings > Home Screen Order starts with a strip of zone templates (`HomeLayout`,
  `HomeLayoutEditor`); drag the four large sections (Severe, Thumbnails, Forecast, Text) into zones, or click a chip for a
  zone menu. `MainWindow::arrangeColumns` builds a `QGridLayout` of zones; section contents/order are still set by
  the older per-item controls. Prefs `HOME_LAYOUT_TEMPLATE` / `HOME_LAYOUT_ASSIGNMENT`. The old column-order pref is
  now unused. Drag-and-drop itself is untested (no GUI here); the click-menu move and the saved state were.
- **Toolbar styles:** Settings > Toolbar Order has a style (icons / icons + names grouped / menu bar) and editable
  groups (rename, add, delete, reorder, per-entry group; reset to built-ins) - `ToolbarGroups`, `Toolbar::rebuildButtons`.
  Route items now have unique ids (two icons were shared; saved orders key on the id, same as before for the first).
- **Middle-button drag** pans the home page (`MiddleDragScroll`, tested with synthetic events). Left-drag scrolling
  was already on (`QScroller`).
- **Windows gdal-data.zip** shipped and CI-verified (GDAL reads its tables through /vsizip/).
- Dev aid: `WXQT_GRAB=<png>[,<ms>]` saves the main window picture and quits (use `QT_QPA_PLATFORM=offscreen`).

## Home screen thumbnails for any tool (2026-10-01)
`HomeThumbnails` (src/util) is the registry of home-screen thumbnails: each entry has a token (pref key), a label, the
Toolbar route it opens on click, whether it is drawn on white, and how its picture is made. Added (all off by default):
SPC Day 1/2/3/4-8 outlooks, storm reports, fire outlook, compmap, WPC rainfall outlook, OPC, Global GOES, Lightning
(GLM), NHC Atlantic outlook, and **RRFS GRIB - your last field and region** (rendered from the newest run; the GRIB
viewer now saves `GRIB_LAST_FIELD` / `GRIB_LAST_REGION`). SPC mesoanalysis, soundings, compmap and GRIB renders are
composited on white (`UtilityUI::updateImage(..., white)`). Not done: MRMS / REFS / SHIP thumbnails (SHIP renders take
minutes cold), per-thumbnail field choice, thumbnails for text-only tools. Right-click Save now works on these.

## Severe outlook comparison (2026-10-01)
`SvrComparison` (toolbar icon ntor.png, Severe weather group): SPC outlook / CSU-MLP (schumacher.atmos.colostate.edu) /
ABPG analogs (University of Missouri, https://analog.missouri.edu/ANALOG/threats.php) side by side for days 1-8, after
the NWS St. Louis "CIPS/CSU/SPC Comparison" page. **CIPS (SLU) is no longer updated** (files last changed 2026-08-26) and
was dropped in favour of ABPG: regional maps (8 regions, region combo remembered in `ABPG_REGION`) of the % of the top 15
analogs with 1+ / 5+ severe reports, F024..F144 = days 1-6, runs 00Z / 12Z; the picture addresses and the newest run are
read from the page. Each panel shows its valid time: SPC days 1-3 from the outlook page ("Valid 011300Z - 021200Z"),
ABPG = run + forecast hour (the pictures say "Valid at ..."), CSU-MLP and SPC days 4-8 derived from the file time
(marked "from file time"); an ended period is marked EXPIRED so a stale picture cannot look current.
Dev aid: `WXQT_OPEN=<toolbar id>` opens a tool headless, `WXQT_GRAB` pictures it.

## Export formats (2026-09-30, local)
All save buttons go through `UtilityAnimationExport`: Animated PNG built in;
WebP always (img2webp is bundled in both packages - Windows `tools/`, AppImage
`usr/bin` - ffmpeg is the fallback); JPEG XL / AVIF / MP4 offered when cjxl /
avifenc / ffmpeg are installed (not bundled - keeps packages small). Choosing
WebP opens an options dialog (lossy / lossless / mixed, quality, effort, sharp
YUV, frame delay, pause on last frame, play count), remembered in prefs. **GIF is deliberately not supported** (user decision).
Failures are always shown in a message box - never a silent fallback.
Every `Photo` / `Image` picture (WPC national images, observations, OPC,
storm reports, soundings, compmap, outlook summaries, dashboard thumbnails,
...) has a right-click "Save image..." through the same exporter; the source
bytes are stored on the label itself (`wxqtSourceBytes`) because those
objects live inside vectors. Left-click only triggers the click action now.
Hovering a picture shows "Updated <time> UTC / Right-click to save".
**Timestamps:** `URL::getBytes` records the server's Last-Modified (else the
fetch time) in a small in-memory table keyed by a hash of the image bytes
(`URL::metaFor`), so anything holding just the bytes can name the file by the
picture's real time: default save names are `yyyyMMdd_HHmmZ_<product>` (UTC),
e.g. `20260930_1630Z_spc_convective_outlooks_day1otlk_1630.png`; an animation
uses its newest frame's time. Screens showing several pictures add the
picture's own file name. **Model images we render** (GRIB, REFS, SHIP/STP incl. max composites, SPC
Post) are named `<run date>_<cycle>z_f<hour>_v<valid time>Z_<product>_<region>`,
e.g. `20260930_06z_f012_v20260930_1800Z_rrfs_tmp2m_conus.png`; an animation or
max composite gives ranges (`f012-f024`, `v...Z-...Z`). Built by
`UtilityAnimationExport::modelName` from each screen's own status line (valid =
run + hour, UTC). RTMA (an analysis) uses `validName`: `20260930_1600Z_rtma_*`.
Not done: writing the time into the file's metadata.

## Python dependency in rendering — removed
No render pipeline uses `gdal_calc.py`/`gdal_merge.py` any more. Nodata
transparency is a leading `nv 0 0 0 0` color-table line +
`gdaldem color-relief -alpha`; C->F is `gdal_translate -scale`; SHIP/STP's
formula math runs in C++ (`calcRaster` in `UtilitySevereIndices.cpp`,
rasters round-tripped through GDAL's ENVI driver, -9999 propagated).
Verified pixel-identical (or within a few alpha-ramp pixels) vs the old
output for GRIB, SPC Post, REFS, RRFS background, SHIP and STP. Visible
change: alpha-0 low-end color stops (low CAPE, low wind, ...) are now
genuinely transparent. Not yet verified: a portable build run on a machine
with no Python/osgeo at all.

## Active — Derived Severe Indices (SHIP / STP / Parametric viewer)
Full plan: `docs/derived-severe-indices-plan.md`. Segments 1-6 all done and
verified (field mapping, derived grids, SHIP formula, HAILCAST contour
overlay, production backend, `IndexViewer` window + toolbar entry, STP fixed
layer as a second `Index` row). Along the way: fixed a NOMADS rate-limit
self-pacing gap (now a shared `URL.cpp` throttle, see `wxqt-networking-fixes`
memory), a latent truncated-GRIB2-cache bug in `UtilityGrib::fetchFieldSlice`,
a subtle nodata-masking bug specific to multi-input formulas, and a "Latest"
resolving to the wrong (non-synoptic) run. **Note**: `IndexViewer`'s
on-screen render was never click-tested (this session can't do GUI
automation) - backend is proven for both indices (STP verified live
2026-09-14), UI mirrors the already-shipped `GribViewer` pattern, but that's
not the same as having watched it work. Remaining:
- **True effective-layer STP** — deferred (user: "save the effective
  layer" for later). Needs a full vertical-profile parcel computation
  (effective inflow layer, ESRH, EBWD) RRFS doesn't ship as ready fields -
  substantially bigger scope than fixed-layer STP or SHIP. Not started.
- **24hr-max compositing - DONE (2026-09-30, local)**: `UtilitySevereIndices::
  renderMax` + a "Max of range" button in `IndexViewer` (uses the same
  From/To Range pickers as Play; cap 48 hours). Each hour's grid comes from
  the normal `computeShipGrid`/`computeStpGrid` (grib slices cached), the
  max is one `calcRaster`, then the shared `finishIndexRender` (extracted
  from `render()`, verified pixel-identical). Verified: composite max ==
  largest single-hour max for SHIP (0.702) and STP (1.537) over F01-F06;
  6 hours ~5-9 s once cached. No HAILCAST contours on a composite. Not
  wired to animation/save. "Day 1 max (12z-12z)" preset button added to both
  IndexViewer and GribViewer (`UtilityGrib::day1Hours`); a full cold 24-hour
  SHIP max for the 06z run took ~3 min.

## GRIB viewer max composite (2026-09-30, local)
`UtilityGrib::renderMax` + a "Max of range" button in `GribViewer`: pixel-wise
max of the selected field over the Range hours (same From/To pickers as Play,
cap 48). Meant for swaths - updraft helicity, reflectivity, gust, max temp.
Not offered for contour / wind-barb / station-plot fields. Verified: 3-hr UH
swath from a finished run (peak 199 m2/s2). Note a still-uploading "Latest"
run fails with an explanatory message, not silently.

## Pinned, low-priority follow-ups
- **SHIP field-fetch batching, part 1 - DONE (2026-09-14)**: the actual
  general fix wasn't SHIP-specific - `UtilityGrib::fetchFieldSlice` was
  re-downloading the identical `.idx` text once per field, for *any* caller
  fetching multiple fields from the same file. Fixed with a permanent,
  process-lifetime cache keyed by URL (a run's `.idx` is immutable once
  published). Verified: SHIP's 2 idx URLs each fetched once now instead of
  ~9 and ~4 times - roughly halves the request count for a 13-field render.
  See `wxqt-networking-fixes` memory.
- **SHIP field-fetch batching, part 2 - DONE (2026-09-14)**: general
  `UtilityGrib::fetchFieldSlices()` added alongside the single-field
  `fetchFieldSlice()` - groups a caller's wanted fields by source file, sorts
  each file's resolved `.idx` byte ranges, and clusters records whose gap is
  under a waste threshold into one ranged GET, splitting the response back
  into per-field slices at the already-known offsets (no GRIB2 message-length
  parsing needed - the `.idx` already gives exact boundaries).
  `UtilitySevereIndices::fetchAndWarpAll` now calls this instead of looping
  `fetchFieldSlice` per field. Verified live against a real RRFS run: SHIP's
  `sp`+`terrain` (`PRES:surface`/`HGT:surface`, adjacent in the idx) and
  `ushr6`+`vshr6` (`VUCSH`/`VVCSH:0-6000`, also adjacent) each merged into one
  GET; `t700`/`t500` (not adjacent) stayed separate - 6 fields fetched via 4
  GETs instead of 6, all resulting cached files valid GRIB2 (`GRIB` header,
  `7777` trailer, correct byte-exact sizes). See `wxqt-networking-fixes`
  memory.
- **Watches/warnings overlay — explicitly declined by the user** (2026-09-13:
  "not the short circuit warnings that are valid for a partial hour
  typically" - a per-hour-cached render can't represent something valid for
  only part of that hour without being misleading). `Watch`/`PolygonWarning`/
  `Warnings` exist but this is live NWS feed data anyway, not a static
  resource - not revisiting unless asked again.
- **RRFS/REFS operational cutover** — `para` → `prod` is a single-token flip
  (`UtilityGrib.cpp`, `rrfsStream` constant), NET 2026-10-14, not yet due.
  Re-confirm the date closer to the day; `para` may vanish early as the
  signal it happened.
- **`ObjectAnimate` retrofit - mostly DONE (2026-09-30, local)**: new
  `UrlAnimation` (AnimationBar + ZoomImage) now drives GoesViewer, GoesGlobal,
  SpcMeso and RadarMosaic - play / scrub / From-To range / save (APNG->JXL),
  frame labels from URL timestamps, frames downloaded only on Play/Save/scrub.
  NOT converted: the NEXRAD level-2 screens (`ObjectAnimateNexrad`) - their
  loop re-draws native radar data through `NexradWidget` per frame rather than
  showing a list of images, so it does not fit an image-frame bar; that would
  need frame capture or a bar variant around `NexradWidget`. `ObjectAnimate.
  {h,cpp}` are now unused but left in place so merges from upstream stay clean.
- **AWS RRFS mirror fallback - DONE (2026-09-30, local)**: `URL.cpp` retries a
  failed NOMADS `rrfs/` or `refs/` request (RRFS, RRFS Ensemble, REFS) against
  `noaa-rrfs-ops-pds`, same relative paths. Verified same file size and
  identical `.idx` byte offsets on both, so mixing sources is safe (a few idx
  label strings differ, e.g. QPFFFG records, which only matters for idxMatch on
  those). Collapsed three copy-pasted request blocks into one. Not covered:
  NOMADS directory listings used to discover "latest run".
- **GEOS-FP viewer** — separate global/aerosol viewer (smoke/dust/AOD),
  lowest priority of the GRIB-sources survey.

## Recently closed (for reference, not action)
- **Pre-public-push privacy pass (2026-09-14)**: found while scoping ahead
  of pushing this repo to the user's own GitHub/GitLab. `GlobalVariables::
  appCreatorEmail` hardcoded the user's real personal email, both in source
  and actively sent as part of every outbound HTTP request's User-Agent
  header (NWS/NOAA API usage guidelines ask for a contactable one, but the
  actual address is the user's choice, not something to bake in). Fixed by
  splitting the one constant's two unrelated jobs: `GlobalVariables::
  appOrgName` (a new, neutral, stable `"wxqt"`) now covers `QSettings`'s
  organization key - changing that key would have silently orphaned every
  saved preference (confirmed real accumulated state existed under the old
  email-keyed settings path, migrated to the new `wxqt`-keyed one, old file
  left in place as a backup) - while the actual contact email is now
  `Utility::readPref("CONTACT_EMAIL", "")`,
  empty by default, editable via Settings > General > "Contact email for
  weather API requests (optional)". Also added `*.log` to `.gitignore`
  (`build.log` had local absolute paths sitting untracked at repo root -
  harmless since untracked, but easy to gitignore properly) and moved the
  Windows-portable-packaging work to a separate project (see the "Windows
  packaging" section above).
- `UtilitySpcPost::render()`'s nodata-alpha bug (opaque-black instead of
  transparent for both true nodata and its own <10% "no signal" bin) — fixed
  and verified 2026-09-12, see `wxqt-spcpost-rrfs-background` memory.
- `labelContours` duplication — moved from `UtilityGrib.cpp`'s anonymous
  namespace to a public shared method rather than forking a copy, per
  standing style preference — see `wxqt-code-style` memory.
- Main render resolution was a flat 3200/1600 columns regardless of domain
  size — now "1 pixel per native 3km grid cell" (true 1:1) for SPC-meso/"My
  Area", half that for CONUS/NA — see `wxqt-grib-viewer` memory.
- NOMADS rate-limit self-pacing (shared `URL.cpp` throttle) and a truncated-
  GRIB2-cache detection bug in `fetchFieldSlice` — see `wxqt-networking-fixes`
  memory.
- `UtilityGrib::fetchFieldSlice` re-fetched the identical `.idx` text once
  per field for any multi-field caller — now cached permanently (immutable
  once published) — see `wxqt-networking-fixes` memory.
- SPC Post's boundary combo replaced with 8 independent-toggle checkboxes
  (State/CWA/County/Highway/Lake/Canada/Mexico/Cities) — the line layers
  reused data already bundled for the Nexrad radar screen, zero new
  sourcing needed; city labels needed a real bug fix along the way
  (`CitiesExtended` was double-negating already-correct longitudes,
  probably also silently broken on the Nexrad radar screen) — see
  `wxqt-spcpost-viewer` memory.

## Model sounding (done 2026-09-30, first version)

- `src/sounding/` — SHARPpy-based engine (attribution: `docs/sharppy-notice.md`):
  thermo, profile, parcels (SB/ML/MU + effective inflow), shear/Bunkers/SRH, STP/SCP/SHIP,
  DCAPE, lapse rates, PW. Verified against SPC's printed values on 270 observed soundings.
- `src/models/UtilityModelSounding` builds an RRFS column (prslev 1000-100 mb every 25 mb +
  2dfld surface) at a point; `src/models/SoundingViewer` shows Skew-T, hodograph and the
  parameter table; GribViewer: click the map, press **Sounding** (locked to the selected
  run and forecast hour). One run/hour ~265 MB, cached.
- Done since: area-mean mode (15/30/60 km), SPC observed mode with site/time pickers, convective
  temperature, DCAPE to 258/270 exact, Sounding button in the GRIB, index, REFS and SPC Post
  viewers (SPC Post matches the latest synoptic RRFS run to the image's valid time), regression
  test in CI (`tests/sounding/`).
- Not done: a forecast-hour slider (hour stays locked by choice), SHIP differs from SPC on
  25/160 soundings (cause unknown), the 49 convective temperatures that come out 0.5-1.5 C low.

## Run-to-run change maps (done 2026-10-01)

- GribViewer **Compare** dropdown (off / vs run -6, -12, -24 h): `UtilityGrib::renderDifference` subtracts the
  same valid time from the older run (its lead hour is longer by the same amount) onto the region grid and
  colours it with a symmetric blue-white-red table scaled per field units (`differenceColorMap`). Works with
  the hour slider / animation; Max-of-range resets it. Temperatures are shown as a change in F (x1.8, no +32).
- Shared along the way: `colorizeToPng` (the tail of the max-of-range render) and `warpFieldSlice`.
- Index viewer (SHIP/STP) now has the same Compare dropdown (`UtilitySevereIndices::renderDifference`, 2026-10-01; builds, NOT yet run against live data). Not done: the same control in the SPC Post viewer; model-to-model differences
  (needs the multi-model work that is on hold); a user-chosen comparison run instead of fixed -6/-12/-24 h.
