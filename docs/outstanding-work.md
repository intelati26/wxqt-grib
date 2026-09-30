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
Still-open size levers: slimming the MSYS2 GDAL dependency closure (or a
minimal GDAL build), zstd/xz for the AppImage, stripping the Linux binary.

## Export formats (2026-09-30, local)
All save buttons go through `UtilityAnimationExport`: Animated PNG built in;
JPEG XL / AVIF / animated WebP / MP4 offered when cjxl / avifenc / ffmpeg are
installed (not bundled - keeps packages small; bundling avifenc/cjxl is a
possible follow-up). **GIF is deliberately not supported** (user decision).
Failures are always shown in a message box - never a silent fallback.
Every `Photo` / `Image` picture (WPC national images, observations, OPC,
storm reports, soundings, compmap, outlook summaries, dashboard thumbnails,
...) has a right-click "Save image..." through the same exporter; the source
bytes are stored on the label itself (`wxqtSourceBytes`) because those
objects live inside vectors. Left-click only triggers the click action now.
Discoverability is right-click only - a visible button/hint could be added.

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
