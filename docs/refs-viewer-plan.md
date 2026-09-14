# REFS Ensemble Viewer — plan

Status: **active again, 2026-09-14 (corrected)**. Briefly paused the same day
on a wrong finding: checking only NOMADS/AWS's `refs.*` tree (ensprod-only -
mean/sprd/pmmn/lpmm/prob/eas/avrg/ffri) I concluded individual members
weren't published anywhere. **The user pointed out the actual member tree**:
`.../rrfs/para/rrfsens.<date>/<cycle>/m001/` through `m005/` (NOMADS) and the
matching `rrfsens.<date>/<cycle>/m00N/` prefix on the live `noaa-rrfs-ops-pds`
S3 bucket - a sibling directory to `refs.*`, not a subdirectory of it, which
is why the first check missed it. Verified 2026-09-14: **5 members
(m001-m005)**, CONUS 3km, forecast hours out to **F060**, a reduced-but-rich
per-member field set (`rrfs.tHHz.mNNN.2dfldnomads.3km.fHHH.conus.grib2` -
REFC, CAPE/CIN, MXUPHL, HLCY 0-1km/0-3km, VUCSH/VVCSH 0-1km/0-6km, GUST,
10/80/160/320m wind, precip/snow/freezing-rain, cloud fields - `ENS=+N`
tagged in the `.idx`). This is everything Stage 2/3 (member fetch, paintball
plot on REFC or MXUPHL exceedance) actually need - **un-pausing**, original
plan below is back in play as written. The `prob` file's pre-computed
neighborhood-probability thresholds (found during the brief pause) are still
a real, independent win for Stage 4, just no longer the *only* path there.

Original planning notes below. Companion to the RRFS `GribViewer`
(`src/models/GribViewer.{h,cpp}`, `src/models/UtilityGrib.{h,cpp}`), which is
deterministic-only. This plan is for a new viewer that adds the thing
`GribViewer` fundamentally can't do - "REFS" as this doc (and the app UI)
uses it loosely covers two related but distinct NCEP systems (see "Data
layer" below for the precise split once you need it): the **RRFS Ensemble**
(5 raw, individually-fetchable members) and **REFS** proper (a blended
derived-product system built from a wider time-lagged pool). Either way,
there's spread/agreement information to show (comparing members, or reading
REFS's own pre-computed spread/probability) that a single deterministic run
never has.

## What's actually new here (vs. just "point GribViewer at REFS files")

1. **Member comparison** — the whole reason to build this separately.
   Concretely:
   - **Paintball plot**: each of the 5 **RRFS Ensemble** members' exceedance
     region for a threshold (classic case: reflectivity ≥ some dBZ) drawn as
     a translucent blob in a colour unique to that member, all overlaid on
     one map. Where blobs stack, you see member agreement; where they don't,
     you see spread.
   - **Probability (%)**: probability of exceeding a threshold at each point —
     either read directly from REFS's derived `prob` product, or (fallback)
     computed as `members-exceeding / 5` per grid cell from the raw RRFS
     Ensemble members if the shipped `prob` bands don't cover the
     threshold/field the user wants. **These two are not the same
     ensemble** - REFS's own `prob`/`eas` are blended from a ~14-source pool
     (RRFS deterministic + RRFS Ensemble, both time-lagged, plus HRRR - see
     "Data layer" below), not just the 5 raw members this viewer can fetch -
     a from-members probability is a real, smaller ensemble in its own
     right, not a from-scratch reproduction of REFS's own number. Comparing
     the two once both exist is a legitimate spread/sanity check, not a bug
     hunt if they disagree.
   - Also cheap/free once the plumbing exists: ensemble mean, spread,
     probability-matched mean (pmmn/lpmm) — these are just more `Field`
     entries pointed at REFS's `ensprod` files, same pipeline as `GribViewer`.
2. **Point graphs (a third window)** — click a point on the ensemble map and
   open a small new window with a time-series chart: one line per member
   (a "spaghetti"/plume plot) across the forecast hours, for whatever field is
   selected. This is the payoff of having individual members at all — a
   single map frame only shows spread at one instant; the plume shows how
   spread evolves over the forecast.
3. **Four-panel layout** (2026-09-10 refinement) — the main window is a 2x2
   grid, each cell independently choosing member/product, all four locked to
   one **master forecast-hour control**. See "Layout" below.
4. **Export as full-res JPEG XL / animated JXL** (2026-09-10 refinement) — see
   "Export formats" below. Verified working on this machine (`cjxl` 0.12.0).

## Layout: four-panel comparison

Replaces the single-map idea from the first draft of this plan. Shape:

- **Shared across all four panels**: run combo, region combo, field combo
  (the physical variable — e.g. reflectivity, 2m temp), and **one master time
  control** — reuse `AnimationBar` for this (scrub/play/range/save UI +
  `TimeLine`, unmodified), since comparing members only makes sense over the
  *same* area/variable/time.
- **Per panel** (`RefsPanel`, new small widget — a `ZoomImage` + a compact
  header): a **product combo** — Mean / Spread / PMM / Probability /
  Paintball / "Member N" — and, only for "Member N", a member-index picker;
  only for Probability/Paintball, a threshold field. Default layout on first
  run: panel 1 = Mean, 2 = Probability, 3 = Paintball, 4 = "Member 1" (a
  reasonable comparison out of the box), all overridable per panel.
- **Linked crosshair**: because all four panels render the same region at the
  same pixel size, one hover's `(fx, fy)` fraction is valid in all four.
  `RefsViewer` fans a single `hovered(fx,fy)` out to all four panels'
  `ZoomImage::setMarker` and samples each panel's own `.grid` sidecar — one
  cursor position, four independent read-outs (mean/spread/prob/member all
  telling you about the same point at once, which is the actual point of a
  four-up ensemble view).
- **`AnimationBar` as a UI adapter, not a frame store**: the bar's
  `setFrames()`/`frameAt()` model is one `QByteArray` per timestep, which
  doesn't fit "4 images per timestep." Don't fight that — keep `AnimationBar`
  purely for its scrub/play/range/timeline *mechanics*, and let `RefsViewer`
  hold its own parallel array (`vector<array<QByteArray, 4>>`, or a small
  `PanelFrameSet` struct) indexed the same way the bar reports frames
  (`globalLabelIndices` from `setFrames`), so `onFrameShown(localIndex)` just
  pushes each of the 4 bytes into its panel instead of one image into one
  `ZoomImage`. Feed the bar a cheap placeholder (e.g. panel 1's bytes, or
  nothing at all if that turns out unnecessary) purely so its own frame-count
  bookkeeping stays happy — it doesn't otherwise care what the bytes are.
  If this adapter feels awkward once written, the alternative is genericizing
  `AnimationBar` to carry an opaque payload type instead of hardcoding
  `QByteArray` — bigger change, only worth it if the adapter is actually ugly
  in practice.
- **Rendering cost**: one master-time step now means up to 4 renders instead
  of 1. Render the 4 panels for a given hour **concurrently** (they're
  independent GDAL invocations); keep the existing sequential-across-hours
  sweep for the animation range (so total concurrent GDAL processes stays
  bounded at "number of panels", not "panels x hours"). Cap the export range
  sweep length accordingly — `maxAnimFrames` may need to come down from 24
  now that each frame is 4x the GDAL work.
- **Hover sample-grid cost scales with area, not linearly** (learned fixing
  the equivalent thing in `GribViewer` on 2026-09-10 — see
  [[wxqt-grib-viewer]]): a fixed sidecar resolution means total cell count
  (render + parse time) grows with the *square* of a region's linear size if
  you naively try to keep km/cell constant across region sizes. `UtilityGrib`
  now picks the sidecar column count from the region's span (fewer columns
  for CONUS/NA-sized regions, more for zoomed SPC-meso/"My Area" ones) rather
  than one fixed number for every region - `UtilityRefs` should use the same
  `sampleGridColumns()`-style tiering (probably the literal same helper) for
  each panel's grid, otherwise a 4-panel CONUS view would be even more
  expensive than one `GribViewer` CONUS render already was before that fix.

## Export formats: full-res JPEG XL / animated JXL

Verified working on this machine: `cjxl`/`djxl`/`jxlinfo` 0.12.0 are installed
(`libjxl` package). Tested `cjxl <apng> out.jxl -d 0` on a hand-built 3-frame
APNG — it correctly encodes a **looping animated JXL** (reads APNG input and
keeps every frame + its duration); `-d 0` is mathematically lossless.

- **New `src/objects/UtilityJxl.{h,cpp}`** (GPLv3, mirrors `UtilityApng`'s
  role): `UtilityJxl::available()` (checks `cjxl` on PATH, same pattern as
  `UtilityGrib::gdalBinDir()`), `UtilityJxl::encode(const QByteArray& pngOrApngBytes,
  const QString& outPath, bool lossless = true)` — writes the input bytes to
  a temp `.png` (a valid APNG *is* a PNG, so one code path handles both
  static and animated input), shells `cjxl <temp> <out> -d 0` (lossless) or
  `-q 90` (visually lossless, smaller) and cleans up the temp file.
- **Reuses `UtilityApng` as the intermediate**, not a parallel encoder: build
  the APNG in memory exactly as `GribViewer`/`Rtma` already do for their
  animated-PNG export, then hand those bytes to `UtilityJxl::encode` instead
  of (or in addition to) writing them out as `.png`. Static export is the
  same idea with a single-frame PNG instead of an APNG.
- **"Full res" is already the case for every export in this app** — the
  frames saved today are the actual rendered PNGs (3200px warp), never the
  downscaled `.grid` hover sidecar or a display-scaled copy, so JXL export
  inherits that for free; nothing to change there.
- This is **generically useful right now**, independent of the REFS work -
  `GribViewer::onSave()` and `Rtma::onSave()` could offer "Save as JXL" next
  to the existing PNG/APNG save immediately, using the exact same
  `UtilityApng::fromFrames()` output they already build. Worth doing as its
  own small task rather than waiting on REFS.
- **For the four-panel view specifically**: "single" full-res JXL means one
  file for the *whole comparison*, not four. Composite the current 4 panel
  images into one mosaic canvas first (`QPainter`, 2x2 quadrants + thin
  divider lines + a small caption strip per quadrant naming its
  member/product), then JXL-encode that single canvas. For an animated
  export: build one mosaic per included forecast hour (reusing
  `AnimationBar`'s existing range picker to bound how many), feed the
  sequence of mosaics to `UtilityApng::fromFrames()`, then that APNG to
  `UtilityJxl::encode()` — same two-step pipeline, just fed multi-panel
  mosaic frames instead of single-panel ones.

## Data layer — RESOLVED 2026-09-14, from the authoritative source

Everything below was pieced together from live probing and web-search
snippets that partly conflicted; **superseded** by pulling the actual text
of NWS Service Change Notice 26-48 (Updated, 2026-09-09) directly. This is
the authoritative source (nco.ncep.noaa.gov/pmb/products/refs/ has the full
variable/encoding tables but SCN 26-48 has the structural facts needed here)
and it reveals a naming distinction this plan had been blurring:

**"RRFS Ensemble" and "REFS" are two different, separately-named systems**,
not one thing with two names:
- **RRFS Ensemble** (`rrfsens.*`) - **5 raw forecast members**, varying
  IC/LBC/physics relative to the deterministic run (no separate "control"
  member - all 5 are perturbed variants). 00/06/12/18z, to F060, same NA
  domain as deterministic RRFS at the **same resolution** (3km CONUS/AK,
  2.5km HI/PR) - confirms the user's resolution observation directly from
  the source. Path: `rrfsens.YYYYMMDD/CC/m00#/rrfs.tCCz.m00#.
  {2dfld|prslev}nomads.{3km|2p5km}.fFFF.{conus|ak|hi|pr}.grib2`. This is
  what Stage 2/3 (member fetch, paintball) actually pull from.
- **REFS** (`refs.*/ensprod`) - a separate **product-generation system**,
  not itself an ensemble with fetchable members. Per the SCN verbatim: "The
  REFS is an ensemble product generation system that **combines the
  forecast output from the current and 6h old cycles of RRFS deterministic
  (1a) and ensemble (1b) systems** to generate ensemble products. For the
  Alaska and CONUS REFS domains, **the HRRR provides two additional members
  (from current and 6h old cycles)**." So `mean`/`sprd`/`pmmn`/`lpmm`/
  `avrg`/`prob`/`eas`/`ffri` are blended from a pool of up to **14**
  time-lagged/multi-model contributors (2 deterministic + 10 RRFS-Ensemble
  time-lagged (5 members × 2 cycles) + 2 HRRR) - **not** simply computed
  from the 5 `rrfsens` members alone. This is the actual meaning behind the
  `:prob fcst 0/14` notation already seen in `refs.*`'s own `.idx` files
  (Segment/pause-era probing, `MXUPHL`/`REFC`/etc "Neighborhood
  Probability" records) - "14" is the pool size, not a typo or an unrelated
  threshold count.

**Real design implication for Stage 3/4** (the point actually worth
flagging): a paintball plot or from-members probability built by this
viewer necessarily uses only the 5 `rrfsens` members - a real, honest, but
**smaller and different** ensemble than the ~14-source pool backing REFS's
own official `mean`/`prob`/`eas` bands. They will not necessarily agree,
and that's expected, not a bug to chase - comparing them is a genuinely
interesting sanity/spread check, not a correctness test. The UI should
probably label things precisely: "RRFS Ensemble" for the 5-member
paintball/"Member N"/plume panels, "REFS" for the `ensprod`-backed
Mean/Spread/PMM/Probability panels, rather than calling everything
"REFS" - avoids implying the 5-member paintball is what backs the
probability panel when it isn't.

**Prod path** (post 2026-10-14 cutover, per the SCN): `rrfs/prod/` and
`refs/prod/` replace `rrfs/para/`/`refs/para/` - same single-token flip
`UtilityGrib.cpp`'s existing `rrfsStream` constant already does for the
deterministic viewer, `rrfsens.*`/`refs.*` file-naming otherwise unchanged.
Confirmed still 403 on a live probe today (2026-09-14, cutover a month out) -
re-check closer to the date, don't hardcode `prod` yet.

Also newly relevant from the SCN, not previously known: `ffri` (flash flood
and recurrence-interval exceedance probability, CONUS only) - a real
candidate field for a future SPC-Post-style flash-flood comparison, noted
for Stage 1's field table, not pursued now.

## Architecture — reuse, don't rebuild

Everything below already exists and this viewer should reuse it as-is:

| Piece | Where | Reused for |
|---|---|---|
| `AnimationBar` | `src/ui/AnimationBar.{h,cpp}` | forecast-hour scrub/play/range/save, unmodified |
| `ZoomImage` (+ hover/crosshair/marker) | `src/ui/ZoomImage.{h,cpp}` | map display; the hover value read-out built this session applies directly to any single-field render (mean/spread/prob) |
| `UtilityApng` | `src/objects/UtilityApng.{h,cpp}` | animated-PNG export of a forecast-hour range |
| GDAL shell-out pattern, region table, cache dir | `UtilityGrib.cpp` | same `-te`/`-ts`/`gdaldem color-relief` pipeline for every REFS derived field |
| Sequential-render-sweep pattern (`renderNextAnimFrame`, self-chaining `FutureVoid`) | `GribViewer.cpp` | needed again for paintball (N member renders) and for the point-graph (N members × M forecast hours of point samples) |
| `DownloadParallelBytes` | `src/objects/DownloadParallelBytes.{h,cpp}` | grabbing all member byte-ranges for one field+hour in parallel (same pattern as RTMA) |

New pieces:

- **`src/models/UtilityRefs.{h,cpp}`** — mirrors `UtilityGrib`: `Field` table
  (mean/spread/pmmn/lpmm/prob/eas as simple fields, reusing the same colormaps
  where the units match, e.g. reflectivity/temp - **confirmed compatible**:
  `UtilityGrib::fields`' own reflectivity entry already keys off
  `:REFC:entire atmosphere`, the exact same `idxMatch` prefix seen in both
  `refs.*`'s `prob` records and the `rrfsens.*` member files, so
  `reflColorMap` is directly reusable, not just "probably matches"),
  `render()` for a single
  derived product (same shape as `UtilityGrib::render`, pointed at `refs.*`
  files), plus new member-aware entry points:
  - `renderPaintball(fieldIndex, threshold, regionIndex, forecastHour, runId, ...)`
  - `fetchMemberSeries(fieldIndex, lon, lat, runId, ...)` → per-member value at
    one point across all forecast hours (for the plume graph)
- **`src/models/RefsViewer.{h,cpp}`** — the outer window. Top bar: run /
  region / field combos + the single master `AnimationBar`; body: a 2x2 grid
  of four `RefsPanel`s; an Export action (current view / animate range,
  PNG+APNG or JXL+animated-JXL, single-panel or full 4-panel mosaic). Owns
  the fan-out for the linked crosshair and the `AnimationBar`-as-adapter
  frame bookkeeping described above. A "Graph" button (not overloading
  double-click, which `GribViewer`/`ImageViewer` use for fullscreen) opens
  `RefsPointGraph` at the last hovered/clicked point.
- **`src/models/RefsPanel.{h,cpp}`** — one grid cell: a `ZoomImage` + a
  compact header with its own product combo (Mean/Spread/PMM/Probability/
  Paintball/"Member N") and, conditionally, a member-index picker or
  threshold field. No forecast-hour control of its own - driven by
  `RefsViewer`'s master `AnimationBar`.
- **`src/models/RefsPointGraph.{h,cpp}`** — the third window. A small
  `Window` with one hand-drawn `QPainter` line chart (no new dependency —
  matches how `GribViewer::buildLegend` already hand-draws with `QPainter`;
  Qt Charts isn't currently linked in `wxqt.pro` and pulling it in for one
  plot isn't worth it): X = forecast hour, Y = value, one line per member in
  the same colour key as the paintball plot, plus a bold mean line. Title
  shows the clicked lon/lat.
- **`src/objects/UtilityJxl.{h,cpp}`** — see "Export formats" above.

## Paintball rendering — mechanics

For each member: warp to the region bbox (same `-te`/`-ts` as today) →
`gdal_calc` threshold mask (`--calc="1*(A>=T)"`, nodata-aware) → colorize that
member's mask with its assigned colour at low alpha (e.g. `gdaldem
color-relief` with a 2-stop table: 0→transparent, 1→`memberColour,alpha=110`)
→ composite all N member PNGs with `QPainter` (`CompositionMode_SourceOver`)
onto one canvas, CWA/state lines burned in last so they stay visible through
the paint. A fixed member→colour table (5 RRFS Ensemble members, confirmed
live - leave a little headroom, say N ≤ 8, in case that count ever changes)
drawn as a small legend key
(reuse the `Photo legend` slot pattern from `GribViewer`, or extend
`buildLegend`-style code to draw swatches + member labels instead of a
gradient bar).

## Point graph — mechanics

On click: capture (lon, lat) from the same fx/fy math already built for
`GribViewer`'s hover (`ZoomImage`'s marker fraction → geo coords). For each
member and each forecast hour in the run's range: either (a) reuse a cached
per-member `.grid` XYZ sidecar per hour if one was already rendered for the
paintball/member view, sampling it the same way `GribViewer::onHover` does
now, or (b) render on demand via the sequential-sweep pattern if nothing's
cached yet (bounded the same way `GribViewer::onRangeRequested` bounds its
sweep — cap total member×hour renders, e.g. members × min(hours, 20)).
Result: `vector<vector<double>>` (member → per-hour value), passed to
`RefsPointGraph`.

## Staged milestones

- **Stage -1 — DONE (2026-09-10)**. `UtilityJxl` (`src/objects/UtilityJxl.{h,cpp}`)
  built and wired into every existing image-save site app-wide, not just the
  two REFS-adjacent viewers: `GribViewer`, `Rtma`, `SpcPostViewer`, and the
  general-purpose `ImageViewer`. All now save `.jxl` (static or animated,
  reusing `UtilityApng`'s output as `cjxl`'s input) with a fallback to the
  original PNG/GIF bytes if `cjxl` isn't installed. See [[wxqt-jxl-export]].
  `RefsPanel`/`RefsViewer` (once built) get this for free by following the
  same `onSave()` shape.
- **Stage 0 - DONE (2026-09-14)**. New `src/models/UtilityRefs.{h,cpp}`:
  reuses `UtilityGrib::Field`'s shape verbatim (its `product` slot
  repurposed to mean the `ensprod` type - "mean"/"pmmn" - rather than
  "2dfld"/"prslev"), `UtilityGrib::regionBbox`/`mainRenderColumns`/
  `gdalBinDir`/`idxByteRange`/`resolveSynopticRun`/`synopticRunOptions`
  wholesale (the last two newly promoted from `UtilitySevereIndices`'
  private copies to shared `UtilityGrib` statics - a third copy of that
  logic would have repeated the exact "duplicating reachable logic"
  antipattern flagged earlier this project), and `UtilityGrib::tempColorMap`/
  `reflColorMap` (also newly promoted from anon-namespace-private to public)
  for a visually-consistent scale with the deterministic RRFS viewer. Two
  Stage 0 fields: ensemble-mean 2m temp, and (real finding, not assumed -
  see below) probability-matched-mean composite reflectivity. New
  `src/models/RefsPanel.{h,cpp}` (one grid cell: field combo + `ZoomImage`)
  and `RefsViewer.{h,cpp}` (2x2 grid of panels + shared run/region combos +
  one master `AnimationBar`, the adapter plumbing described above -
  `vector<array<QByteArray,4>>` alongside the bar's own single-`QByteArray`
  bookkeeping). Toolbar entry added (reuses `grib.png`, no dedicated icon).

  Real finding while wiring the first field: **REFS does not produce a
  plain ensemble-mean reflectivity field** (verified live - the `mean` file
  has no REFC record at all, 75 fields, none of them reflectivity) - not a
  bug, presumably because averaging dBZ directly isn't meteorologically
  sound the way it is for temperature. Reflectivity lives in `pmmn`
  (probability-matched mean) instead, tagged `wt ens mean` in that file's
  own `.idx` - used that instead, correctly labeled in the UI as
  "Probability-Matched Mean," not "Ensemble Mean."

  Also caught and fixed before it shipped: the URL was first built under
  `com/rrfs/para/refs.*` (wrong - that returns 404, the whole directory
  doesn't exist there) instead of the correct, separate top-level
  `com/refs/para/refs.*` path - `rrfsens.*` (RRFS Ensemble, the 5 raw
  members) and `refs.*` (REFS's own blended `ensprod` products) are
  siblings under different top-level dirs (`com/rrfs/` vs `com/refs/`),
  not one nested under the other. Caught by the live render returning
  "not available" and checking the URL directly rather than assuming the
  code was right.

  Verified end-to-end 2026-09-14 against a real REFS run (`2026091406`,
  F01/F06, CONUS): both fields render as valid 867x385 RGBA PNGs with
  correct nodata/echo masking (sent to the user directly - see
  `wxqt-refs-viewer-plan` memory). Also constructed `RefsViewer` itself
  under a real `QApplication` event loop (not just `UtilityRefs::render()`
  standalone) and confirmed the full async 4-panel pipeline completes
  without crashing, with correct cache-hit dedup across panels sharing a
  field (2 PNGs on disk for 4 panels, as expected). Found and fixed one
  real bug along the way: connecting the panel field-combos before setting
  their default indices fired 3 redundant concurrent `resolveSynopticRun()`
  probes on first launch - reordered defaults-before-connect, same fix
  shape as elsewhere in this codebase.

  **Known Stage 0 simplifications, not silently dropped**: the 4 panels
  render sequentially per hour, not concurrently (the plan's eventual
  target - deferred, not forgotten); there is no linked crosshair/hover
  read-out yet (RefsPanel doesn't sample a `.grid` sidecar at all yet);
  `onSave()` is a no-op (mosaic export is Stage 6). Same GUI-click-testing
  caveat as `IndexViewer`: this session can't do GUI automation, so the
  actual on-screen 2x2 layout was never visually confirmed in the running
  app - confidence rests on the constructed-under-QApplication test plus
  the independently-verified render pipeline, not on having watched it
  render.
- **Stage 1** — round out the single-file derived products: `sprd` (spread),
  `pmmn`/`lpmm` (probability-matched mean, localized), `avrg` (SCN-confirmed:
  "a combination of the pmmn and mean fields"), and `prob`/`eas`
  (probability / ensemble-agreement-scale). All confirmed real product types
  straight from SCN 26-48, no more guessing what ships. Pure `Field`-table +
  pipeline work, same shape as everything already in `UtilityGrib.cpp`.
  Panels can now show Mean/Spread/PMM/Probability for real. `ffri`
  (flash-flood/recurrence-interval exceedance, CONUS only) is a real fifth
  product type too, worth a row but not required for Stage 1 to be "done."
- **Stage 2** — member fetch/cache plumbing: parallel byte-range download of
  all members for one field+hour (`DownloadParallelBytes`, RTMA-style),
  cached per run+field+hour+member like `g_*.grib2` today. Panels can now
  show "Member N"; linked crosshair becomes actually interesting (four
  different members' read-outs side by side).
- **Stage 3** — paintball renderer + member-colour legend key.
- **Stage 4** — probability-of-exceedance UI: threshold picker; direct render
  from `prob`/`eas` bands where available, from-members fallback where not.
- **Stage 5** — `RefsPointGraph` (the third window): click-to-sample, plume
  chart.
- **Stage 6** — the 4-panel mosaic export (single JXL / animated JXL of the
  whole comparison, per "Export formats" above), `AnimationBar` range-export
  applied per-panel too, toolbar entry (new icon; button placed near "RRFS
  GRIB Viewer" in `src/ui/Toolbar.cpp`).

## Open questions to settle at Stage 0

- ~~Exact member file path/naming~~ **RESOLVED 2026-09-14**:
  `.../rrfs/para/rrfsens.<YYYYMMDD>/<cycle>/m<NNN>/rrfs.t<cycle>z.m<NNN>.
  2dfldnomads.3km.f<FFF>.conus.grib2` (`<NNN>` zero-padded 001-005), same
  shape on the operational-track `noaa-rrfs-ops-pds` S3 bucket. A `prslevnomads`
  file exists alongside 2dfld the same way RRFS deterministic splits
  2dfld/prslev. AK/HI/PR domains also present (2p5km for hi/pr, 3km for ak),
  matching `refs.*`'s region set.
- ~~Whether `prob`/`eas` thresholds cover what users actually want~~
  **RESOLVED 2026-09-14**: yes - REFC ≥10-50 dBZ, MXUPHL >25/75/150, WIND
  gust thresholds, precip/snow/frz-rain all present in `refs.*`'s `prob`
  file. RRFS Ensemble members ALSO carry MXUPHL/REFC/CAPE directly, so a
  from-members probability is a real second option, not just a fallback -
  see the "Data layer" section above for why the two won't necessarily
  match (different, differently-sized source pools).
- ~~Member count~~ **RESOLVED 2026-09-14**: **5** RRFS Ensemble members
  (`m001`-`m005`), confirmed live (`m006` 403s) and directly stated in SCN
  26-48 ("The RRFS produces five ensemble forecast members"). Not yet
  confirmed constant across every cycle/domain -
  check more than one run before hardcoding "5" anywhere load-bearing.
- ~~Region set~~ **RESOLVED 2026-09-14**: the premise behind this question
  was wrong - REFS is **not** coarser than RRFS deterministic. User
  observation, confirmed against the file naming above: REFS members run at
  the same **3km** grid as deterministic RRFS (both say `.3km.` in the
  path), because per the BAMS description REFS *is* literally an ensemble of
  RRFS runs (perturbed ICs/physics + time-lagged members), not a separate
  coarser model the way SREF was relative to the old NAM/GFS. No
  resolution-based reason to trim the region set - reuse `UtilityGrib`'s
  full `regionTable` as-is, SPC-meso zoom sectors included, same "1 pixel
  per native grid cell" 1:1 tiering `mainRenderColumns()` already does for
  RRFS.
- **Nearest-neighbour snap vs. bilinear interpolation for the hover value**
  (raised 2026-09-10, unresolved - a tension, not a decision): the earlier
  ask was explicitly "a known cell versus guessing," which is what the
  current snap-to-cell-centre + crosshair-on-that-cell behaviour gives.
  Bilinear interpolation between the four surrounding cells would read
  smoother and arguably closer to the "true" value at the exact cursor
  point, but it stops being "this is what cell X says" and starts being
  "this is a computed blend nobody can point to on the grid" - the crosshair
  would have nowhere honest to sit. If interpolation is still wanted, it
  should probably be a *second* number ("interpolated: 41.2°F") next to the
  snapped cell's value, not a replacement for it — keeps both the honesty
  and the smoothness. Applies equally to `GribViewer`'s existing hover and
  whatever `RefsPanel` does.
