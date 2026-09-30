# Feature scope — existing and new (2026-10-01)

Scoping only; nothing here is started. Effort: **S** = hours, **M** = about a day,
**L** = several days, **XL** = a week+. "Verify" says how it can be proven without
a GUI session, in the way the rest of this project has been checked.

State of the app at this point: 41 toolbar screens (radar, GOES, SPC/WPC/NHC/OPC
products, model image viewers, RTMA, RRFS GRIB viewer, SPC Post, SHIP/STP index
viewer, REFS ensemble viewer); Windows zip 47 MB, AppImage 66 MB; one shared
save/export path; Python-free rendering; AWS fallback for model data.

## A. Finish or deepen what exists

| # | Item | Effort | Why it matters | Notes / verify |
|---|------|--------|----------------|----------------|
| A1 | **NEXRAD loop export** (save the radar loop as APNG/JXL/AVIF/WebP) | M | The one animation left without the shared bar/export; radar is the most-used screen. | The loop re-draws native data through `NexradWidget`, so frames must be captured (`grab()` per step) — check GL-offscreen works first. Verify: capture N frames headless, feed `UtilityAnimationExport`. |
| A2 | **Observation Sites as a native widget** | M | Lets QtWebEngine be deleted for good (the `--webengine` build option, the placeholder). Was your "down the line" item. | Source data: the NWS obs-timeseries JSON behind the two web pages. Verify: fetch + render offscreen. |
| A3 | **Write the valid time / run into saved files** (PNG `tEXt`, JXL/AVIF metadata) | S | File names carry it now; metadata survives renaming. | Verify: read back with `exiftool`/PIL. |
| A4 | **Automated regression tests in CI** | M | There are none. Every check so far was a throw-away harness in a scratch dir. Promote the useful ones: name builders, threshold snapping, `calcRaster`, idx parsing, render-to-PNG golden checks on a tiny fixture GRIB. | Highest leverage for keeping everything else stable. |
| A5 | **Effective-layer STP** (deferred by you) | L | The operational STP. Needs a parcel/vertical-profile computation from the `prslev` file. | Only when you provide/approve the formula; nothing guessed. |
| A6 | **REFS leftovers**: `eas` / `ffri` products | S–M | Flash-flood and "equal-area" probabilities. | Blocked on confirming what the published thresholds mean — do not label a flood product on a guess. |
| A7 | **Desktop polish**: remember window sizes/positions per screen; keyboard shortcuts list; a first-run hint for right-click Save | S | Small daily-use wins. | — |
| A8 | **Packaging**: single-file Windows exe (self-extracting), Windows code-signing, optional auto-update check | M / cost / M | See the single-file discussion; signing costs money. | SFX first; signing only if distribution widens. |

## B. New features

| # | Item | Effort | Why it matters | Notes / verify |
|---|------|--------|----------------|----------------|
| B1 | **Multi-model GRIB viewer** (HRRR, NAM, GFS, NBM, GEFS alongside RRFS) | L | The viewer only knows RRFS. The byte-range `.idx` fetch, region warp, colormaps and export already generalise; what is missing is a per-model URL/field table. | Survey of which NOAA models fit the idx approach already exists in notes. Verify: render one field per model against live data. |
| B2 | **Point meteogram / sounding** — click a point, get a time series (any GRIB field) and a model sounding (skew-T + hodograph + key parameters) from the `prslev` file | L | Forecasters' most common "what happens at my location" question. The plume chart code (`RefsPointGraph`, `gdallocationinfo` sampling) is the template. | Skew-T is the big part. Verify: compare parameters to a known sounding. |
| B3 | **Difference maps**: run-to-run and model-to-model (new minus old, valid-time matched) | M | "What changed since the last run" is a core workflow; trivial with `calcRaster` + a diverging colormap. | Verify: diff of a run with itself = 0 everywhere. |
| B4 | **More derived indices** beyond SHIP/STP: 0–1 km SRH/shear composites, SCP, significant-hail variants, DCAPE, lapse-rate maps | M each | Reuses the `IndexViewer` + `calcRaster` pipeline; needs the inputs RRFS publishes. | Formulas come from you or published references; none invented. |
| B5 | **Multi-day max/accumulation products** generalised: "total QPF over range", "peak gust swath" as saved presets | S | The Max-of-range / Day 1 12z–12z machinery exists; presets and sums (not just max) are small. | — |
| B6 | **GEOS-FP smoke / dust / AOD viewer** | M | Lowest priority in the original survey. | New data source. |
| B7 | **Alerts as notifications** (desktop toast for warnings/watches for the saved locations) | M | You declined the *map overlay* (short-fuse polygons misleading in a per-hour image); notifications are a different thing — still, ask first. | Needs a background poll + a setting. |
| B8 | **Saved views / bookmarks**: store a screen + product + region + hour combo and recall it | S–M | One click to your usual dashboards. | Pref storage already exists. |
| B9 | **Share / report export**: one multi-panel image or PDF from several screens | M | The REFS mosaic export generalised. | — |

## C. Recommended order

1. **A4 tests** (M) — protects everything below.
2. **B2 point meteogram first, sounding second** — biggest new capability, reuses the most code.
3. **B1 multi-model** — unlocks B3/B4/B5 for more models.
4. **B3 difference maps + B5 presets** — cheap wins once B1 lands.
5. **A1 NEXRAD loop export** and **A2 native Observation Sites** — closes the "animations" and "no web engine" threads.
6. **A5 effective-layer STP** when you are ready to provide the formula.

## D. Decisions (user, 2026-10-01)

- **Multi-model (B1): on hold.** RRFS/REFS stays the scope; RRFS "will kind of eat
  everything". Revisit later; B3/B4/B5 proceed on RRFS alone.
- **Sounding (B2): SHARPpy-style full parameter set** - not the simple skew-T. This
  is the next big feature (see the plan below).
- **Notifications (B7): interesting, later.** Current focus is working through the
  visual dashboard; do not build notifications yet. (The "no short-fuse map overlay"
  decision stands.)
- **Windows single file (A8): not wanted.** The 47 MB zip is fine.

## E. Sounding plan (B2) - scoped, not started

Inputs: RRFS `prslev` (isobaric T/RH/U/V/HGT at the model levels) + surface fields
(2 m T/Td, 10 m wind, surface pressure, terrain) at a clicked point and forecast hour,
through the same idx byte-range fetch + `gdallocationinfo` sampling the plume chart
uses. All computation in C++ (the app stays Python-free).

1. **Profile builder** - merge surface + pressure levels into one sorted profile;
   Td from RH; wind speed/dir from U/V. (S)
2. **Thermodynamics** - virtual temperature, LCL, parcel lifting on moist
   adiabats (surface-based, mixed-layer 100 mb, most-unstable, forecast), CAPE/CIN,
   LFC/EL, 0-3 km CAPE, DCAPE, lapse rates, PWAT, freezing/wet-bulb-zero heights,
   downdraft parameters. (M-L; the accuracy-critical part)
3. **Kinematics** - bulk shear layers, Bunkers storm motion, SRH (0-1, 0-3,
   effective), mean winds, critical angle. (M)
4. **Composite indices** - STP (fixed and effective), SCP, SHIP, EHI, significant
   hail/tornado parameters, K/TT-style legacy indices. (M; formulas from the
   published definitions only - anything not certain is left out, not guessed)
5. **Effective inflow layer** (this also unlocks effective-layer STP, A5). (M)
6. **Display** - hand-drawn skew-T log-p (temperature, dewpoint, parcel trace,
   CAPE/CIN shading, wind barbs), hodograph with storm motion and SRH, a parameter
   table, time-slider over forecast hours. (L)
7. **Verification plan** - compare every parameter against an independent
   reference (MetPy / SHARPpy) on the same soundings, including real RRFS points
   and known textbook profiles; tolerance documented per parameter. The reference
   is a development-time oracle only and never ships.

Rough size: L-XL overall; steps 1-4 first (numbers before pictures), so the
accuracy is settled before any drawing work.
