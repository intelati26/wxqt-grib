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
- **Eventual, not yet staged**: 24hr-max compositing mode (pixel-wise max of
  SHIP across an hour range) — the user's actual end target, sitting
  alongside the SPC human forecast and SPC Post 24hr probability as a third
  lens on the same threat. Needs Segment 5's shell to exist first, but the
  shell's hour control should be designed with this in mind from the start.

## Active — RRFS Ensemble / REFS viewer
Full plan: `docs/refs-viewer-plan.md`. Stages -1 and 0 done. Briefly paused
2026-09-14 on a wrong finding (only checked NOMADS/AWS's `refs.*`
ensprod-only tree); **user pointed out the actual per-member tree**,
`rrfsens.<date>/<cycle>/m001/`-`m005/`, a sibling of `refs.*` not a
subdirectory - confirmed live on both NOMADS and the operational-track
`noaa-rrfs-ops-pds` S3 bucket. Per SCN 26-48 (authoritative), **"RRFS
Ensemble" and "REFS" are two different systems**: RRFS Ensemble = 5 raw
members (`rrfsens.*`, what paintball/plume will pull from); REFS = a
product-generation system (`refs.*/ensprod`) blending a ~14-source pool
(RRFS det + RRFS Ensemble, both time-lagged, + HRRR), not just the 5
members - so a from-members paintball/probability will be a real but
different, smaller ensemble than REFS's own `prob`/`eas`, expected to
disagree somewhat. Resolution confirmed identical to deterministic RRFS
(3km CONUS/AK, 2.5km HI/PR) for both.

**Stage 0 done (2026-09-14)**: new `UtilityRefs`/`RefsPanel`/`RefsViewer`,
2x2-panel shell + `AnimationBar`-as-adapter plumbing, verified live against
a real REFS run (ensemble-mean 2m temp + probability-matched-mean
reflectivity, both valid 867x385 RGBA renders). Along the way: promoted
`UtilityGrib::resolveSynopticRun`/`synopticRunOptions`/`gdalBinDir`/
`idxByteRange`/`tempColorMap`/`reflColorMap` from private/duplicated to
shared public statics (fixed a real pre-existing duplication in
`UtilitySevereIndices` too, not just avoided a new one); caught a wrong URL
(`com/rrfs/para/refs.*` doesn't exist - the real path is the sibling
top-level `com/refs/para/refs.*`) and a real REFS-side finding (no plain
ensemble-mean reflectivity field exists - use `pmmn` instead). No GUI
click-testing available this session - see `wxqt-refs-viewer-plan` memory
for the full verification chain. Toolbar entry added. Next: Stage 1
(round out mean/sprd/pmmn/lpmm/avrg/prob fields), then Stage 2 (member
fetch plumbing against the now-confirmed `rrfsens.*` tree).

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
- **`ObjectAnimate` retrofit** — `GoesViewer`/`SpcMeso`/`Nexrad` still use the
  older `ObjectAnimate`/`ObjectAnimateNexrad` classes (plain loop, no zoom,
  no slider scrub, no save). Bringing them up to the `AnimationBar` standard
  already used by RTMA/GribViewer/SpcPostViewer is a separate retrofit, not
  yet requested.
- **AWS RRFS mirror fallback** — `noaa-rrfs-pds` as an automatic fallback
  when NOMADS 429s/403s, no UI needed. Noted, not implemented.
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
