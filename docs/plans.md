# Plans

What was planned and what is left. Check the code before acting on a line.

**Contents**

- 1. Fold the NSSL tar loader into the main GRIB path (done)
- 2. NetCDF sources: what could be combined (SFCOA / RRFSD done)
- 3. One GDAL raster reader for the NetCDF (and tar) flows (done, but the GOES layer)
- 4. Model verification against MRMS (reflectivity)
- 5. Open-Meteo's `.om` files on S3: grids with no API calls (done 2026-11-05, `d864da8`)
- 6. One model screen over two canvases (the Model Viewer and the master map)
- 7. WOOF-NAM from its own site (a hosted source for the model that is already a local one)
- 8. Extended point view: a saved place's 11-day forecast from the NBM text bulletins (plan, 2026-11-12; nothing written)
- 9. Charts as presets of the checks (plan, 2026-11-13; nothing written)
- 10. One model's field over another model's chart (idea, 2026-11-13; nothing written)
- 11. Taking `render()` apart, and one rule for where things are drawn (plan, 2026-11-13; steps 1 to 4 are done, 2026-11-14)
- 12. Android build (APK) and GDAL - notes (2026-10-30)
- 13. Derived severe-weather indices (SHIP, SIGTOR/STP) — plan
- 14. GFS charts: what is missing, and how anomalies could work
- 15. History: an archive tier and one fetch path (plan, outline)
- 16. REFS Ensemble Viewer — plan
- 17. Sounding viewer: remaining SPC panels (sketch, 2026-10-01)
- 18. Tropical Hub - idea and sketch (2026-10-02)

**Done (2026-11-02):** items 1 and 3 and the SFCOA / RRFSD piece of item 2, see the notes in each. Left: the `oneWhole` split and the SPC REFS Zarr question of item 2, and the GOES history layer's own GDAL steps (item 3).

## 1. Fold the NSSL tar loader into the main GRIB path (done)

Done: `TarReader` (tar walk by range and unpack), `GdalRaster::bandOf` (the band by the NOAA names), `kind = tar` with `tar_member` in the definition files, and the two MPAS models are definition files; `MpasNsslLoader` is gone. Check: MPAS-RN and the REFS paintball with the MPAS runs draw as before. The old `mpas-nssl` cache folder is no longer read and can be deleted.

**Today:** `src/gfs/MpasNsslLoader.cpp` (MPAS-HT, MPAS-RN) is a custom loader (`kind = custom`). It walks the tar's 512 byte headers by range
(`findMember`), downloads and gunzips one hour (`hourFile`), lists the bands with `gdalinfo` (`bandsOf`), and cuts and warps each field with
`gdal_translate` / `gdalwarp` (`load`). The main GRIB path (`GfsData.cpp`) already has the warp (`Source::warp`), the scale and offset, the cache, the
run search (`probeFile`) and the parallel fetches.

**Plan:**
1. Move the tar walking and the gunzip into a shared `TarReader`: "member X of this tar", by range requests, with the index kept in the cache.
2. Add a container option to the model definition file (`docs/model-files.md`, `GfsModelFile`): `container = tar.gz`, `member = {tag}_nssl_{date}{cycle}f{hour2}.grib2.gz`.
3. Add a second way to index a file: with no `.idx`, the record list comes from the unpacked file (what `bandsOf` does), and a want can say which band
   (the loader takes the first band today, which is the unchecked updraft helicity question).
4. Delete `MpasNsslLoader`; MPAS-HT and MPAS-RN become two plain definition files. Other NSSL tars (NSSL-WRF, HRRRE and the others on their server) then
   cost a definition file, not C++.

**Cost:** each hour is still a download of about 100 MB (tar members cannot be fetched record by record). The cache names change, so MPAS hours are fetched once more.
Re-check the MPAS charts, the paintballs and the pooled charts afterward.

## 2. NetCDF sources: what could be combined (SFCOA / RRFSD done)

Done: the shared lines of SFCOA and RRFSD are a built-in piece (`_spc_post`, `include = spc_post`); the two models read the same as before (the golden test's blocks for them are unchanged).

Sources not read through the `.idx` path: GEOS-FP, ERA5, SFCOA, RRFSD (all `kind = netcdf`, read through GDAL's `/vsicurl/` by range), SPC REFS (a Zarr store,
`src/zarr/` and `src/spcrefs/SpcRefsLoader.cpp`, about 1400 lines of C++ with its own Blosc decoder), EC46 and CFS30 (`Ec46Loader`, `Cfs30Loader`).

- **SFCOA and RRFSD:** the same SPC post-processing, same warp grid (0.03 degrees, CONUS), same server and same prod/para url pair. They differ in hours and
  run rules. One shared template (a `[include]` or a `base =` line in the definition format) would keep the urls, warp and sectors in one place.
- **Whole-file GRIB and NetCDF:** `oneWhole` in `GfsData.cpp` handles GEPS-style whole files, NetCDF by range and custom loaders in one long function. Splitting it
  into a small reader per kind (whole GRIB, range NetCDF, partial, custom) with one shared tail (the stats, scale and offset, the cache write) would make
  the tar container of item 1 a fourth reader, not a fifth special case.
- **SPC REFS Zarr:** GDAL has a Zarr driver, but the minimal vcpkg build (`.github/vcpkg/vcpkg.json`: tools, geos, png, sqlite3, openjpeg, netcdf, curl) does not
  include it and the AppImage and Windows packages use that build. Adding the `zarr` feature (and its Blosc codec) would let REFS be a `kind = netcdf`-style
  definition read through `ZARR:"/vsicurl/..."` and drop `ZarrStore` and `Blosc`. Check first whether GDAL's Zarr reader handles the REFS store's
  chunking, its Lambert grid and the Blosc compression, how fast it reads the 5 members by range, and what the extra feature adds to the package size.
- **GEOS-FP OPeNDAP (`GeosOpendap`):** a `partial` hook, kept as is; the same hook could serve a THREDDS or OPeNDAP source for another model.
- **EC46 and CFS30:** not looked at for this plan; read `Ec46Loader.cpp` and `Cfs30Loader.cpp` to see whether they are a format of their own or GRIB/NetCDF with a layout the definition file cannot describe.

## 3. One GDAL raster reader for the NetCDF (and tar) flows (done, but the GOES layer)

Done: `GdalRaster` (`src/gfs/GdalRaster.cpp`) is used by `GfsData` (three places), the climatology (three) and `SarWindsLayer`. `GoesHistoryLayer` makes a scaled picture with its own chain of `gdal_translate` and `gdalwarp` calls to GeoTIFF and PNG, not an ENVI grid, so it stays as it is.

The same steps are written out at least five times: run `gdal_translate` or `gdalwarp` on an input (a file, or `NETCDF:"/vsicurl/<url>":<variable>` read by range),
write ENVI (`.raw` + `.hdr`), read the header for the size and origin, swap the byte order if the header says so, map the missing value to NaN, and delete the scratch files.

- `GfsData::oneWhole` (GEOS-FP, ERA5, SFCOA, RRFSD, GEPS) with `readEnvi`
- `MpasNsslLoader::load` (its own copy of the header parse and the byte swap)
- `GfsClimate.cpp` (three `NETCDF:/vsicurl/` reads of the 1991-2020 climatology)
- `SarWindsLayer.cpp`, `GoesHistoryLayer.cpp` (map layers from NetCDF, with their own `gdal_translate` calls)

**Plan:** one helper (`GdalRaster::read(input, {band, warp box and step, range-read options, missing value}) -> Grid`) next to `UtilityGrib`, and move these five onto it.
What differs stays in the callers: the input name, the band choice, the scale and offset, the caching. Do this before item 1, so that the tar reader has a reader to feed.
SPC REFS (Zarr) and the SPC and obs NetCDF products read by `UtilityMadis` are not on this path.
EC46 and CFS30 are not NetCDF either (EC46 is the Open-Meteo JSON service, CFS30 is read through `UtilityCfs`), so they stay as they are.

## 4. Model verification against MRMS (reflectivity)

First version done (2026-11-02): `src/gfs/MrmsVerify.*` and `src/models/MrmsVerifyViewer.*`, in the Analysis menu on reflectivity charts; the instantaneous and hour-maximum comparisons, the threshold scores, the FSS at 10 / 25 / 50 km, the three pictures. Left: a drawn box instead of the whole chart area, the live (newest run) mode, a lead-time graph, charts that are hourly maxima on the model side (HRRR `sim_radar_max`). A Model Viewer entry (Analysis menu, "Verify against MRMS...") that scores a model's reflectivity against MRMS at a valid time on the hour (:00Z).

- **Pairing:** pick a date and hour (UTC). Every run with a forecast for that hour supplies a field (the 18Z valid time is f06 of 12Z, f12 of 06Z, and so on). MRMS is the scan nearest :00 (scans come every 2 minutes, `UtilityMrms::scansAwsAt`), with its real time shown.
- **Two kinds of comparison:** the instantaneous composite reflectivity against the nearest scan, and the model's hourly-maximum reflectivity against the maximum of the MRMS scans across that hour. The screen says which one it is doing.
- **Views:** model, MRMS, the model's 35 dBZ contour over the MRMS fill, and a hit / miss / false alarm map.
- **Scores** (20, 35, 50 dBZ, over the visible area or a drawn box): POD, FAR, CSI, bias, and the Fractions Skill Score at 10, 25 and 50 km. A table with one row per run shows skill against lead time.
- **Open questions:** live (last runs against the latest MRMS) or archive (any past event) or both; which models first (HRRR, REFS, the NSSL ones); the common grid to compare on (the model's, or MRMS's 0.01 degrees).

## 5. Open-Meteo's `.om` files on S3: grids with no API calls (done 2026-11-05, `d864da8`)

**Status:** built as a model source (`kind = om`) with the decoder vendored in `src/external/omfile/` and linked in, not as the helper program described below: the decoder's source files carry "either version 2 of the License, or (at your option) any later version", so they can sit inside a GPLv3 program, and `licenses/CREDITS.md` says so. The EC46 map reads the files instead of the API. The helper-program design below is kept as the fallback if that reading of the licence is ever in doubt. What is left: the models of `docs/open-meteo-models.md` not yet listed in CREDITS (check which of the groups are in), and asking the author to confirm "or later" for the repository's own files.

**Why:** one EC46 map through the API is some 800 requests (up to about 5,000 by the service's weighting) of the 10,000 a day. Open-Meteo also publishes the forecast grids as files in a public bucket, read by plain HTTPS range requests, which do not count against the API allowance.

**The catalog of the models, their grids and where each would sit in the viewer's groups is `docs/open-meteo-models.md`.**

**The source (checked 2026-11-03):** `https://openmeteo.s3.amazonaws.com/data_spatial/<model>/<yyyy>/<MM>/<dd>/<HH>00Z/<valid time>.om`, with `meta.json` in each run folder and `data_spatial/<model>/latest.json` for the newest run (the run time, `completed`, the valid times, the variable names and the grid as WKT). EC46 weekly (`ecmwf_ec46_weekly`): five weekly files of about 10 MB for each daily run, about 50 variables (mean and anomaly of 2 m temperature, precipitation, pressure, wind, SST, soil, snow, cloud, with probabilities of the anomaly passing a threshold, EFI and shift of tails), global, on the reduced Gaussian O320 grid. SEAS5 has no spatial files (only the time series under `data/`), so it stays on the API.

**The reader:** [open-meteo/om-file-format](https://github.com/open-meteo/om-file-format), the C library (about 320 KB of source: the file trailer and variable tree, the chunk look-up table, the FastPFor / TurboPFor integer decoder with SSE and AVX2 variants, the 2-D delta filter). Sans-IO: it says which byte ranges it needs and is handed them, so the program's own `NetManager` (ranges, priorities, the Activity counters) does the fetching. A file is chunked, so a regional picture reads only its chunks, not the whole 10 MB. **Licence to settle first:** the repository says GPL-2.0; the bit-packing files (powturbo's TurboPFor) say "version 2 or, at your option, any later version", which fits wxqt's GPLv3, but `om_decoder.c` and the others carry no notice. Ask the author to confirm "or later" (an issue on the repository) before vendoring; the fallback is a reader written from the format's description.

**Grafting it on (a `kind = om` source, as `tar` and `netcdf` were added), with the reader as a separate program:**
The library is GPL-2.0 and wxqt is GPLv3, so the reader is **not linked into wxqt**: it is a small helper executable, `wxqt-om`, built from the library's source and started with `QProcess` the way `gdal_translate` and `gdalwarp` are. It takes a URL, a variable name and a region and writes raw floats and a small header to a file; wxqt reads that. Two programs talking through arguments and files keep their own licences. The helper ships with its source and the GPL-2.0 text (`licenses/`) in the three packages (AppImage, Windows, portable folder), and `licenses/CREDITS.md` says what it is. Still ask the author to confirm "or later" (an issue on the repository): with that answer the decoder could be linked in instead, with no extra process. This is the same arrangement as an editor that loads an encoder it cannot bundle (Audacity and LAME): the program talks to it, it is not part of it.
1. `tools/om/` (or a vendored copy under `src/external/om/` built only as the helper): the C decoder and a `main` that opens a file by HTTPS (the helper does its own range reads: the decoder is sans-IO, so a few lines of libcurl, which is already in the build) and writes the region.
2. `src/om/OmRunner.{h,cpp}` in wxqt: finds the helper next to the program, runs it, reads the result; the cache and run logic stay in wxqt.
3. Definition file: `kind = om`, `om_model = ecmwf_ec46_weekly`, and each chart's field by the Open-Meteo variable name (`temperature_2m_mean`, `_anomaly`). The run comes from `latest.json` (one small request, no listing); a run is the day for EC46.
4. Grid to GfsGrid: regular lat/lon grids copy straight in; Lambert and rotated grids go through the existing warp (`GdalRaster`, the WKT from `meta.json`); **reduced Gaussian (EC46, IFS, AIFS)** needs the Gaussian latitudes (the Legendre roots) and the ring lengths of the octahedral grid (4 x ring + 16 points), then interpolation to a regular 0.25 or 0.5 degree grid (in wxqt or in the helper: the helper is simpler to test).
5. `Ec46Loader` takes the map from the files; the plume at a point stays on the API (a few calls, and kept until the next UTC day).
6. Test: the same week of EC46 from the API at a handful of points against the file's values.

**What it would open (the app has its own GRIB sources for the models marked "have"):**
- New to the app: UK Met Office global 10 km and its 20 km ensemble (`ukmo_global_*`), JMA GSM and MSM, Meteo-France ARPEGE world 0.25 (and AROME France), DWD ICON global (ICON-EU and ICON-D2 are "have"), CMA GRAPES, ECMWF WAM waves, CAMS global (air quality, greenhouse gases), CMC GDPS and RDPS, NCEP hurricane GEFS ensemble mean (`ncep_hgefs025_ensemble_mean`), AIGEFS, and the regional European models (KNMI, DMI, MetNo, MeteoSwiss, ARPAE ICON-2I, GeoSphere Austria) if wanted.
- Have: GFS, GEFS, HRRR, NAM, NBM, RRFS, AIGFS, IFS, IFS ensemble, AIFS, HRDPS, GEPS. These would only be a second source (a mirror when NOMADS is down, or a smaller regional read), and carry only the variables Open-Meteo keeps (no radar reflectivity fields, for one).
- Check before relying on a model: the variables list in its `latest.json`, and the run's lag behind the provider's own files.

## 6. One model screen over two canvases (the Model Viewer and the master map)

**Why:** the Model Viewer works the way the model sites do (model, run, chart and area along the top, step through the hours; the chart is the subject). The master map works by layers (the
map is the subject, the model chart one layer of 38; its model and chart are chosen in the layer's option panel, and the time bar belongs to every layer at once). "On the map..." therefore
opens a second window where the controls just used are gone or moved. Both draw through the same renderer (`GfsRender::png`, `GfsChart`, `GfsData`), so the difference is only in what is
round the chart.

**The idea:** one set of model controls, the **model bar**, over one of two canvases: the **picture** (the titled chart with its legend) or the **map** (the scrollable map with the chart as
a layer). Opened from Models it starts on the picture with the layer list hidden; opened from the master map it starts on the map with the layer list shown. With no model chart on, there is
no model bar and the screen is today's master map.

**The rule for time:** while the model bar is there it owns the time: the bar steps the run's forecast hours and every other layer follows the valid time (the frame at or before it). The
notes under each layer in the layer manager say which layers have nothing at that time. Without the model bar, the map's general time bar is used as now.

### Where it stands (2026-11-10)

Done (committed 2026-11-10; the steps below say what each became):

- `gfs/ChartSelection` (new): the model, the chart, the ticked lines, the other models' lines, the member, standard deviations, the change since an older run, the day's maximum, the area
  and the valid time, as one object with a line form (a saved view's) and the one function that makes the renderer's request (`variant`).
- The map's model layer holds its state through it (`selection()`, `apply()`), its saved-view line and its request to the renderer come from it. Saved views keep their format.
- The handoffs carry it: "On the map..." gives the layer the whole selection; the map's "Other models... > Model Viewer" gives the viewer the lines, other models, standard deviations, the
  change and the member (`ModelViewer::applyVariants`, the member once the model's list is read).
- The master map says what time each layer is drawing against the bar's (a line under each layer that is on, amber when it is behind), what kind of time the bar is on, where "now" is on the
  strip, and "Latest" is the frame of now when forecasts and observed layers are on together.
- Started, compiles, not on screen yet: `ModelChartLayer::controls(parent, changed, asBar)`: the layer's choices as its panel (as before) or as a bar (the same widgets and handlers side by
  side on a flow layout), `ModelChartLayer::bar()`, and `refreshers` so the panel and the bar show each other's changes.

Checked offscreen (screenshots), not by clicking: the bar, the Time box, the handoffs both ways, the Picture | Map box. Not checked: loops and the From-to range with the new "Latest", the compare map's two bars.

### Steps

1. **The model bar on the master map** (the first part done 2026-11-10: the bar is up while the layer is on, with the model, chart, change, colors, build, other models, run, member and
   standard deviations; the panel keeps the draw-ahead and the opacity and shows the rest only when there is no bar; the two stay in step. The area is in the bar and the hour strip is the time bar below it; a saved view restores through `setSettings` -> `apply` -> `optionChanged` -> `refreshControls`, and the combo chart layer has another id so it has no bar (read from the code, not clicked).
   Step 1 is done.)
   - `MasterMapViewer`: a row above the time bar that holds `modelLayer()->bar(...)` while the layer "model/chart" is on, made and removed where the layer manager's rows are (the same
     `changed` callback as the layer's options: `optionChanged`, `redraw`). The layer's panel in the manager then keeps only what is the layer's own (opacity, draw ahead).
   - `ModelChartLayer::optionChanged` calls `refreshControls()` so a choice made in one place shows in the other.
   - The bar gets what the Model Viewer's top rows have and the panel lacks: the area (the map's "Area..." moved into it) and the hour strip (step 2).
   - Check: the bar and the panel stay the same after each choice; a saved view restores both; the combo chart layer (`ComboChartLayer`), which has model layers as its parts, is not given a bar.
2. **The bar owns the time.** (done 2026-11-10: with the model chart on beside other layers a "Time: the model's hours / every frame" box picks whether the strip is the model's forecast hours or every layer's frames; the others show the frame at or before the valid time.)
   - `MasterMapViewer::rebuildTicks`: while the model layer is on, the ticks are its hours only; the others are shown the frame at or before each (they already are: `showLayerAt`). The
     strip is labelled in forecast hours with the valid time beside it; "Latest" stays the hour of now.
   - A way out for the case it takes away: stepping the radar's own scans between two model hours. A choice on the bar, "Time: the model's hours / every frame", the first by default.
   - Check: loops, From-to, the history page's `openAt`, the compare map (two maps whose bars move together).
3. **The Model Viewer on the same selection.** (first part done 2026-11-10: the four fields are one `ChartSelection picked` and `selection()` starts from it; the master map's "Other models > Model Viewer" hands the whole selection over (`ModelScreen::applySelection` -> `ModelViewer::applySelection`: model, chart, area, time, variants; no preferences written first; the Tropical Hub, history page and the Events screen still write them); a change of other than 6, 12 or 24 hours gets its own item in the box, and the day's maximum is taken (waits for the hours if they are not known yet; both built, not clicked); the renderer's request of a plain or change chart comes from `selection().variant` (a maximum keeps the viewer's own hours); left: the model, chart and area from `ChartSelection::variant`.)
   - Its own members (`overlays`, `overlayModels`, `member`, `standardDeviations`, `variant`, the model, chart and area of `objectModel`) become one `ChartSelection`, and its requests to the
     renderer come from `ChartSelection::variant` (the tiles of a compare add their own on top). The differences that stay are named in the object: the difference from another model or
     run, the same valid time across models, the hidden layers of a paintball, the pooled ensembles' choice, the fitted legend.
   - The model, chart and area of a chart handed over are set from the selection, not by writing `NCEP`, `MODELNCEPPARAMLASTUSED` and `MODELNCEPSECTORLASTUSED` first (the Tropical Hub
     and the history page do the same and move to the selection too).
   - The day's maximum comes back from the map; a change of other than 6, 12 or 24 hours is kept, not dropped.
   - Check: `tests/gfs` still pass; each menu of the viewer on GFS, REFS, a storm model, a local area and a picture model; the settings kept between runs.
4. **The bar is one piece.** (2026-11-10: `models/ModelBar` holds what is the same in both places: the model list (grouping, descriptions, main models first), the change box's items and mapping, and Colors...; with `ChartPickers::chooseOverlayModels` and the chart and area pickers these are written once. NOT done, and why: the viewer's model, run, valid-time and chart rows are the lists of `ObjectModel` that every model of the screen shares, the pictures from other sites and SPC pages as well as GRIB models, so they stay the viewer's; a bar widget that replaces them has to wait for the `ObjectModel` of a GRIB model to be a `ChartSelection` (step 3's last part).) The Model Viewer's top rows and the map's bar become the same widget (`models/ModelBar`), bound to a `ChartSelection`, the run and the hour. The layer's
   `controls(..., asBar)` is the first form of it; the viewer's rows (the model box, the run box, the chart button, the area button, the time strip, member, change) are moved onto it one at a
   time, each a commit that changes nothing on screen.
5. **Picture / Map in the Model Viewer.** (done 2026-11-10 in the model screen: a Picture | Map box beside the Source box on the charts page; Map shows the master map with the model layer on the same selection (`ModelScreen::showMap`), Picture takes the map's selection back (`applySelection`); the Viewer's "On the map..." and the map's "Other models > Model Viewer" switch in place inside the screen and open windows as before outside it. Checked offscreen both ways; not by clicking. Left: the picture's compare tiles and fitted legend on the map canvas, the area of a storm model.) A switch under the bar: the picture is replaced in place by the map widget with the same selection as its model layer, and "Add layers..." offers
   the rest of the map's layers. "On the map..." becomes that switch.
6. **One window.** The model screen and the master map are the same screen opened two ways (step 5's switch and the layer list shown or hidden). The source box of the model screen (SPC
   Post, NSSL CAMs, the picture models) stays as pages beside it.

### Remaining work (as of 2026-11-10; steps 1, 2, 3 and 5 are done, 4 is as far as it should go)

In order, each its own commit:

1. **Click it through** (nothing below was clicked, only screenshotted): the Time box with a loop and From-to; "Other models > Model Viewer" and "On the map..." from the real menus; the Picture | Map box on a local area, a storm model and with a member or a change; the Colors, Build and Overlay dialogs from the map's bar.
2. **The MAG page without what a picture can not use** (done: Map, Analysis, Colors and Sounding hidden on it; its GRIB tiles stay).
3. **The last callers off the preferences** (done for the CFS 30 day and WPC PPP buttons of the home screen; left: the "last forecast" thumbnail route (`GRIB_LATEST`), which is the saved choice by design).
4. **Step 4's end, if wanted**: the viewer's remaining rows (member box, standard deviations, Build) read from `ModelBar` too. Little code is still twice; do it only with the next change to those rows.
5. **Step 6 closed (2026-11-10, the user's call): the picture stays a separate part of the app, nearly legacy.** The master map is where the model chart grows (its model bar, the Time box, layers with it); the Model Viewer (the titled picture, compare tiles, saved views, the pages of the model screen) is kept working and gets fixes, not new features. No Picture view inside the master map, no flipping of the windows. The bridge between them stays as built: "On the map..." and the map's "Other models > Model Viewer" hand the whole `ChartSelection` across, and the model screen's Picture | Map box shows the map in its window (to be dropped if it confuses; say so).
6. **The open points below** are answered by the same call: compare tiles, the fitted legend and Save stay the picture's; only the storm models' area on the map is still undesigned.
7. **WOOF-NAM from its site** (section 7 below): a hosted source for the model that is a local one today.
8. **The rest of "Also from this look"**: the model family's pages sharing time and area, the hurricane links, the master map's layer search and shared choices.
9. **Unrelated to the model screen, still open**: the day-2 / per-day marks comparison against a second date of the nice-weather data; HDF5 reader fuzzing; GDAL driver restriction for MRMS, GOES, SAR and drought reads; the intermittent GDAL read error (now reported with its reason, `b1ceabe`).
10. **Nice weather levels against his, by date (2026-11-10):**
   - **His days are one ahead of ours.** His `all_cities_day<N>.json` has a `valid_date`: day 1 is today (Tue 11-10), day 2 is Wed 11-11 (his "Tomorrow"). Our Day 1 is the next daytime after the run (from a 20Z run on Tuesday it is Wednesday), so our Day N is his day N+1. A screenshot of our Day 2 (Thu) lines up with his Thursday (his day 3), not his Tomorrow tab. Consider naming our days by date in the window ("Wed 11-11") so the two can be set side by side.
   - **Wed 11-11, ours Day 1 (the 20Z run, F024) against his day 2 file, 19,908 cities: 64% the same level.** By his level (rows) and ours (columns Decent, Fair, Nice, Beautiful): Decent 2,897 / 2,467 / 140 / 0; Fair 528 / 8,608 / 1,466 / 5; Nice 0 / 61 / 1,193 / 51; Beautiful 0 / 14 / 85 / 19. We are too generous where he says Decent (as many Fair as Decent) and not generous enough at the top (his 124 Beautiful cities: 19 match). His whole day: Fair 10,877, Decent 6,027, Nice 1,371, Beautiful 124, none 2,716.
   - **To do:** the same comparison for Thu 11-12 (our Day 2 against his day 3 file; the download needs the user's yes), then the sub-scores of the cities that disagree (the file by state has them) to see which input (cloud, rain chance, temperature against the normal) separates the Decent/Fair and the Nice/Beautiful boundaries; compare only a run that covers the whole target day (12Z for a next-day target).
   - **Tool:** `WXQT_SAMPLES=<in.csv>,<out.csv>` on the `chartpng` route writes the chart's value at each "lat,lon" line (the cities of his file), for such comparisons. His data stays in the scratchpad, never in the app.

### The pictures of the model guidance site (MAG), apart (2026-11-10)

The model screen's NCEP page lists only the models drawn from GRIB; the models that are only pictures there (RAP, NAM, NAM-HIRES, HiResW, NAEFS, WAVEWATCH, ESTOFS, POLAR ...) are their own page, **NCEP MAG pictures** (`ObjectModel` prefModel `NCEPMAG`, `kind` `NCEP` so the picture code is the same; its choices are kept apart). What it makes possible: the NCEP page is only models whose state is a `ChartSelection`, so step 3's last part and a bar widget need not carry the pictures. Left on the MAG page for now that a picture has no use for: Map, Analysis, tiles of GRIB charts, Sounding, Colors, Build (hide them there).

### Open points

- **Compare tiles.** Four pictures side by side have no map form beyond the map's two maps. Either the tiles stay a thing of the picture canvas, or the map gets panes.
- **The legend.** The picture's fitted legend (the range of what is on view) needs a place on the map canvas; the map's legend strip shows the whole scale.
- **Exports.** "Export view" of the map already saves the Model Viewer's titled picture (`titled()`); with one screen there is one Save.
- **Storm models.** Their area is the storm; on the map the view follows the storm's grid. Not designed.
- **Size.** `ModelViewer.cpp` is 3,800 lines whose rows are tied to its combo boxes and `objectModel`; step 4 is the long one. Steps 1 and 2 give the familiar controls on the map without it.

### Also from this look (not part of the steps above)

- The model family: carry the valid time and the area between the pages of the model screen; fold SPC Post into the Model Viewer (it is there already as the models `SPCPOST` and
  `SPCGEFS`); list the 15 minute window and the analysis loop as pages; open MRMS verification on the chart, area and time on view.
- The hurricane screen and the models: "HAFS model..." was there; "Model charts here..." on the hurricane screen (the GFS's shear on an area round the storm) and "Storm page..." in the Model Viewer's Map menu on a storm model are done 2026-11-10 (built, not clicked; the storm page's year is the calendar year, wrong for a January look at a December storm). Left: HAFS-A and B as a family of the hurricane intensity chart (one intensity chart, not two).
- **Stacked winds on the map (2026-11-10): done on the flat map, not yet seen on the globe.** The master map's model layer now draws one barb set per ticked level (`barbs_850` / `barbs_700` / `barbs_500`: red, green, blue, each next set 12% shorter; `VectorLines::windSets`, `paintStackedBarbs`); seen on the flat map with three sets. Left: (a) the legend does not name the colors; (b) the globe is not verified (the `globemap` and `mrmsat` hooks do not apply `MM_MODELCHART_OVERLAYS`, so a test needs the hook to take `PREF=value` as `mcexport` does, or the layer to read the pref); (c) the globe takes the layer's picture through the Mercator skin, so barbs near the poles are cut at 85 degrees.
- **The poles (2026-11-10):** the globe's Mercator skin stops at 85 degrees and the cap beyond it took the nearest row along each meridian (straight wedges). Now: the interactive globe asks `WorldRender` for a north and a south polar picture of the chart on the map (`MasterMapViewer::ensurePoleCaps`, 20 degrees from the pole, 1400 px, rebuilt when the chart, model or valid time changes) and `GlobeMapView` takes them poleward of 78 degrees (faded in over 78-82); until they are ready, or with no model chart on the map, the cap is the row's blur (a stretch of the row that widens toward the pole). Polar map exports with a model chart on the map are always painted from the grid (`exportWorldTo` switches High resolution on); the grid-painted exports have a white ground (the globe's caps keep the map's background), light lines on a dark ground. Holes: the caps carry the fill only (no contours or barbs past 85); other layers (radar, satellite) are replaced above 82 by the model picture; the first draw of a chart reads the grids twice; the south pole and a zoomed globe are seen only once, and a rendered globe at 86 degrees showed a dark patch near the pole that is probably the data (low humidity) but is not confirmed; `setPolarFrame` is a global the cap render sets and does not restore (a polar export in the same moment would take its reach); the white ground is for the exports only, the map's own background setting still defaults to black; a polar export's outside (beyond the circle) is white, so the circle's edge has no outline.
- The master map: done 2026-11-10: a search box and the layers last used in the Layers window, a layer-opacity slider for every layer but the model chart (kept as MASTERMAP_OPACITY; checked on the radar, by a screenshot); the time bar's "now" word keeps off the names; a loop between two hours of a model run (the looprange hook) steps F000 to F018 as it should. Left: the order of the layers (painting order is fixed by kind), "Show on the map" from the radar, outlook and satellite screens; the search and last used were built, not seen (the Layers window has no hook).

## 7. WOOF-NAM from its own site (a hosted source for the model that is already a local one)

*Done 2026-11-11 as the model `WOOF-NAM-HOSTED` (GfsBuiltInModels.cpp, group Convection allowing; the local run keeps `WOOF-NAM`): idx names are NOAA's, the 0.1 degree warp is NAM-ANL's, and GfsGrid::find treats "entire atmosphere" and "... (considered as a single layer)" as one level. Not done: the run from status.json (the newest cycle is found by the usual probe), hiding charts the files lack, the hindcasts.*

**What the site is** (read 2026-11-10 from its index page, `README.txt` and `status.json`; nothing downloaded yet, so every number is the site's): `https://woof-nam.recastsystems.com`, free forecast files from Recast's WOOF-NAM, a NAM-style model on a GPU at 12 km over North America (not NOAA's: GRIB2 process 254). NCEP grid 218 (Lambert conformal, 614 x 428, 12.19 km, grid-relative winds), the fields and levels of NOAA's NAM `awphys` files (1000 to 50 hPa every 25 hPa; no lightning, cloud-top visibility, Blackadar length or accumulated snow). Four runs a day (00, 06, 12, 18Z) to hour 84, started from the GFS 0.25 degree analysis of the cycle; the first file about 3 h 30 after the cycle, all 84 hours about 4 h 20 after; files deleted after 7 days. CC BY 4.0: credit "WOOF-NAM by Recast"; not an official forecast. No API: plain HTTP.

**Files:** `woof-nam/12km/<yyyymmdd>/<HH>z/woof-nam.t<HH>z.awphys<FF>.tm00.grib2` with a wgrib2 `.grib2.idx` beside it (so a field is a Range read, as the program already does for NOMADS and the AWS buckets), `cycle.json` (the run's files, sizes, SHA-256) and `maps/`. A run is about 5.9 GB (66 MB an hour from the 06Z run of 5 Nov 2026; the earlier layout was 175 MB an hour), so **only by the index and ranges**, never a whole file. `status.json` says the newest run, its state (running, complete, did not finish, skipped; the page and the file disagree on one word, "failed"), the hours available and the next cycle expected: one small request finds the run, no folder listing. Hindcasts of about 25 past events (2011 to 2026) are under `woof-nam/hindcasts/<event>/`, in the same layout with a `cycle.json`.

**The site and the `.idx` of each file** (for the model definition): site `https://woof-nam.recastsystems.com` (README at `/README.txt`, newest run at `/status.json`, a run's page at `/woof-nam/12km/<yyyymmdd>/<HH>z/index.html`). **Every hourly GRIB2 file has its own `.idx` beside it**, the same name plus `.idx`: `.../woof-nam.t12z.awphys24.tm00.grib2` and `.../woof-nam.t12z.awphys24.tm00.grib2.idx`, in wgrib2's inventory form (record number, byte offset, date, variable, level, forecast). That is the program's `index_suffix = .idx` and its usual Range read: the `.idx` is fetched first (small), the wanted records' byte ranges are found from it, and only those ranges of the `.grib2` are asked for. 84 hours a run means 85 `.grib2` and 85 `.idx` files; `cycle.json` lists them with sizes and SHA-256 values, the way to know that a run is whole.

**Why it fits:** NCEP retired the NAM on 2026-11-03 and the program's WOOF-NAM today is the local one (`kind = wrf`, folders on this computer, `source_label = WOOF-NAM (local run)`). The site gives the same model to everyone, as the NAM's replacement in the model list, with no GPU run needed.

**Steps**

1. **A second source of the same model** (the registry's `source` mechanism the other models use for a mirror): `kind = grib` with the site's `url` / `file` (the url `https://woof-nam.recastsystems.com/woof-nam/12km/{date}/{cycle}z/woof-nam.t{cycle}z.awphys{hour2}.tm00.grib2` (tokens of docs/model-files.md; to be checked against the real file names)) / `index_suffix = .idx`, `cycle_hours = 6`, `hours = 0-84/1`, the local run kept as the first source when its folders exist, the hosted one otherwise; `source_label = WOOF-NAM (Recast, hosted)`. Check first what the file's variable and level names are in the `.idx` (wgrib2 names, as NOAA's) so the chart list of NAM's `awphys` charts is reused.
2. **The Lambert grid 218.** The registry reads the NOAA Lambert grids (HRRR, NAM 12 km) through the GDAL warp already: the same path, a 614 x 428 grid; winds are grid-relative and turned east / north as the NAM's are. Test: the same hour from the hosted file and from a local run, compared at a handful of points.
3. **The run:** from `status.json` (the newest complete run, and the hours available while it is still running), not by probing. A run that `status.json` calls running is shown with the hours it has.
4. **Hours and fields that are not there:** the missing fields (lightning, snowfall) hide their charts for this model; the precipitation-type fields only from the 12Z run of 9 Nov 2026, so a chart of them says so for an older run. Hours beyond 84 are not offered.
5. **Credit and manners:** "WOOF-NAM by Recast" in the legend's data line and the model list's text, the not-NOAA note beside it, one request a second and the plain `wxqt` agent, and ask before the first download (as for any new source).
6. **Hindcasts** (a second step, if wanted): the 25 events as archive runs in the history page's model choice, the way the other archives are; the comparison PNGs are the site's own and are not copied.
7. **The model list:** WOOF-NAM goes in the NCEP page's "Convection allowing" group as the NAM's successor; the NAM pictures stay on the MAG page while NCEP serves them.

**Open points:** whether the site keeps this URL layout (it changed layout once, on 5 Nov 2026: check `cycle.json` for the file list rather than assuming 84 files); its capacity (a 5.9 GB run a day times four is theirs to carry: ranges and the preload limit keep the program to the fields on view); the contact in the README is for the site's maker, for any question of use beyond the CC BY terms.

## 8. Extended point view: a saved place's 11-day forecast from the NBM text bulletins (plan, 2026-11-12; nothing written)

**Source of the idea:** `extended-point-view.md`, the summary of the "Extended" page of simplemodelview (Kotlin, checked 2026-11-12 on the live bulletins; Norman OK end to end, the station finder on every station of the 12Z NBX and NBE files). It is not a new data source for the app (the NBM GRIB model reads the same bucket) but the **long-range half of the forecast-point page** plus a hand-off to the Model Viewer.

**What it is:** for a saved place, graphs and a table for hours 0 to 264 from the NBM bulletins of its nearest station: NWS grid for the gap before the first NBS column (3 h samples), NBS 6-72 h every 3 h, NBE 84-192 h every 12 h, NBX 204-264 h every 12 h; temperature with a +/- 1 sigma band, wind with direction arrows, precipitation chances; one column can be picked and a button opens 500 mb heights and winds for that valid time (an SPC REFS run that has it, else GFS).

**What the app has already (read, not run):**
- `misc/UtilityForecastPoint` (`fetch`, `Data::hourly` series keyed `temperature`, `dewpoint`, `skyCover`, `windSpeed`, `windGust` ..., `expand` for the run-length-encoded `validTime`, `durationHours`): the gap table is this data sampled every 3 h; **units differ** (the page's `Data` is in the display units, the bulletins' F, knots, tens of degrees): convert at the one place that builds the table. `windDirection` is not in `parameters()` (check; the spec needs it).
- `misc/ForecastPointViewer` (a `Window`, `QTabWidget` with "Summary and graph", "Hourly table", "All graphs", the place combo, `loadPoint`): the new page is a fourth tab ("Extended"), so the place choice, refresh and saved places are shared.
- `objects/URL::getBytesRangeManaged` (range reads), `models/ModelScreen::openAt(model, validUtc)` / `ModelViewer::openAt(model, validUtc, runStart)` / `selectModel("SPCREFS")`, `models/RefsPointGraph` and `spcrefs/SpcRefsLoader` (the REFS store, for the "has this time" question).
- Not found: any reader of the text bulletins, and a nearest-observing-station lookup (`api.weather.gov/points` -> `observationStations`; the existing fetch goes through the gridpoint link).

**Steps, each testable alone:**
1. **`nbm/NbmText` (new, pure parsing, no network):** the header regex, the column positions from the last digit of each `FHR` number (3 characters ending there, `-99` missing), the CLIMO pair of NBE ignored, `TXN` as 12Z minimum / 00Z maximum, dropping columns with no TMP, SKY, WSP or P12. Tests from real KOKC blocks (NBS, NBE, NBX), copied from the spec's repository, plus a stand-in file with several sorted runs.
2. **`nbm/NbmTextFile` (the one-station finder over range reads):** the newest file by the bucket listing (`?list-type=2&max-keys=1&prefix=blend.<day>/<HH>/text/blend_<kind>.t<HH>z`, back an hour at a time to 12); the 32-window index kept in memory per file; per interval a bisection where ids ascend, 96 KB chunk reads where a break lies inside (the files are several sorted runs: plain bisection missed about 9 %, PASA and PAOM among them); the imaginary `~END~` header at the file's end; the chunk read one window past the end of its interval. Kept on the plan: one listing, up to 32 index reads, then 7-10 reads per place and product. **Test on the real NBX and NBE files for every station, not only on the stand-in** (the stand-in passed while 41 stations still failed).
3. **`UtilityNwsStation` (new):** `points/<lat,lon>` at 4 decimals truncated, `observationStations` nearest first, the first of the nearest 6 whose id the file has; kept per saved place (id, name) in the preferences. The place's coordinates go to `api.weather.gov` once: say so in About (the existing fetch already sends them for the gridpoints).
4. **`nbm/NbmPointTable`:** the merge: API gap columns (from now minus 3 h), all NBS, NBE after the last NBS, NBX after the last NBE, each tagged with its source; valid time = cycle + FHR.
5. **The tab:** equal-width columns so graphs and table line up although the spacing changes; the rows of the spec's section 7 (temperature band, wind arrows pointing where the wind blows to = WDR x 10 + 180, precipitation bars from P12 or P06, snow / ice / thunder / snow level rows only when non-zero); a click on a column selects it and shows the bottom bar with the valid time and the hand-off button. Reload after 30 minutes. Qt side: a custom-painted widget like `ForecastPointChart`, not one `QTableWidget` per graph.
6. **The hand-off:** the valid time more than 66 h ahead or past: GFS. Else ask the newest REFS cycles (8, 6 h apart) whether their time coordinate has a value within 30 minutes of it (the first newest that does), through the REFS loader; found: `ModelScreen::openAt("SPCREFS", valid)` and check the viewer picks that run, else pin it; not found: `openAt("GFS", valid)` and the 500 mb heights and winds chart (`500_wnd_ht`) at the nearest frame. The spec's first version guessed "within 54 h" and was wrong whenever the newest run was still being written: ask, do not guess.

**Cost and manners:** a place is about 10-40 range reads of 4-96 KB once per file (the file is 10-30 MB and holds all 9,589 stations): keep the index and the station block for the cycle in the disk cache, one request at a time per host, the plain `wxqt` agent. A file is new every 6 h (NBS / NBE) and the 13Z run's NBX lists columns to about 372 h with no data past 264: dropping empty columns covers it.

**Not in the first version (as in the source):** mph, precipitation type as colours, a map thumbnail beside the table, Guam's max / min convention, NBP and NBH, the v5.0 additions (WBG). Older cycles' hour lists were not looked at.

**Open points:** where the NBM text bulletins fit with the NBM GRIB model's own run list (the page could say which run it used, as the model does); whether the tab or a window of its own: a tab keeps the place box shared; whether wind in mph needs `windDirection` and the other bulletin elements added to `parameters()` or stays inside the new code.

## 9. Charts as presets of the checks (plan, 2026-11-13; nothing written)

Today the Model Viewer's picker lists every chart of a model (GFS: 62; the picker says 51 once the precipitation ones sit behind "Precip by parts"), and under the list a grid of checks (fill, lines, barbs by level and field) adds fields to the chosen chart. Most of the list is a base fill plus some of those same lines and barbs, so the list can shrink to a few base charts and the grid says the rest.

**A preset** = a base chart id + the ticked check ids (+ the level, for the level charts). Stored as one line per preset in a preference (`MODEL_PRESETS`, `name|model|base|check,check,...`), like the saved views. Built-in presets are the same record, read-only. A favorite is a preset (the star on a built-in saves it under Your presets).

**What folds into the grid** (GFS ids; the checks exist already: `fill_/line_` temp, rh, wind, hgt per level, `barbs_<level>`, `z<level>`, `mslp`, `thick_*`):
- Level charts: `200/250/300/500_wnd_ht`, `850_wnd_ht`, `850/925_temp_ht`, `500/850/700_rh_ht`, `700_temp_mslp`, `850_pw_ht` -> one "Level chart" with a level row (200 ... 925) and the fill chosen by the column (wind, temperature, humidity, height). 13 charts -> 1 base + presets for the usual ones.
- Surface: `mslp_10m_wind`, `10m_wnd_2m_temp`, `2m_dewpoint`, `2m_rh_10m_wnd_derived`, `mslp_pwat` -> one "Surface" base (the 10 m / 2 m row of the grid, plus MSLP lines and 10 m barbs). 5 -> 1.
- Precipitation with lines: the five `*_thick`/`850_temp_mslp_precip`/`10m_wnd_precip` and their five `*_type` twins -> "Precipitation" base with a Type switch and the lines as checks (thickness, 850 mb temperature, 10 m wind). 10 -> 1 (the Precip by parts dialog stays for amount and period).
- Stay as charts of their own (not a fill plus lines): vorticity, divergence, streamlines, shear, steering, theta-e, anomalies, trends, snow depth change, simulated radar, CAPE, the precipitation totals by period.

Result: GFS ~62 -> about 12 "base charts" and 4 to 6 built-in presets (500 wind and height, 850/700/500 wind, Surface, Precipitation + type, ...). The existing "850/700/500 wind" and "850/700 humidity" buttons become two of those built-in presets.

**Picker** (see the two mockups in the conversation): A keeps today's list, shortened, with a "Your presets" group on top and a level row for the level chart; B puts presets as chips above an always-open grid. Either way changing a check turns the name into "Custom: <base> + N changes" with Save as preset... and Reset. Hover on a preset names its checks.

**Compatibility**: saved views, favorites and the model-comparison tiles store chart ids. Every folded id stays valid: the id is read as "base + checks" (an alias table), so nothing saved breaks. The charts a person picks by id from outside the picker (the Extended page's Heights & winds, the Events viewer) keep working.

**Steps**: (1) alias table + Preset record + save/load (no UI change); (2) "Save checks as preset..." and a Your presets group in the current picker; (3) built-in list cut to the base charts and presets; (4) other models (HRRR, NAM, ECMWF) the same, each source with its own base charts; (5) layout B if A is not enough.

**To decide**: layout A or B; whether the level becomes a row of buttons (as drawn) or stays a grid row; whether presets are per model or shared by every model that has the fields (shared is simpler to use: "Chase day" works on GFS, NAM and ECMWF when the fields exist).

**Fill colors in the picker** (asked 2026-11-13, after the 2 m dewpoint fill read as nearly the wind's colors): the grid's fill cells each show a small swatch of their color scale (the strip the legend uses). Clicking a swatch opens the existing Palette editor (stops, colors, transparency, banded or smooth, import/export) on that fill, with a live preview of the chart behind it in the picker, and Apply keeps the scale for that field through PaletteStore, as the chart's own scale is kept today. Today only the chart's own fill can be edited; the ticked fills (`fill_*` overlays, translucent ramps) have none, so the work is giving each overlay fill a store key and having `compose()` read the stored ramp. The same pass decides the stacking: a ticked fill either replaces the chart's own fill (full-size color bar, wind left as barbs and lines) or stays translucent over it; presets store which. Hover read-outs for ticked fills are done (7aeea7b).

**Two fills** (asked 2026-11-13): the picker's Fill row becomes two: a lighter fill (the whole area, a pale scale, drawn first) and a darker fill (a saturated scale drawn over it). Two full-area scales mix into mud wherever they overlap (the dewpoint over the wind), so the darker fill draws only where it matters: from a threshold of its own (dewpoint from 60 F up, wind from 25 kt up, with a default per field that the swatch editor changes), clear elsewhere, so the light fill stays readable underneath. One default scale per field, in a light and a dark version. The legend has two bars (light above, dark below, each with its max marker); the hover read-out has both; a preset stores both fields with their levels and thresholds. The chart's own fill is the lighter fill of a preset that has no other choice. Sizes: the Fill part of the picker is two rows (field, level, swatch, threshold), about 80 px, so the whole picker is about 300 px for fills, lines and barbs.

**Families of contours** (asked 2026-11-13, after a chart with MSLP, 700 and 850 mb heights, three winds' barbs and two fills drawn together): the lines and barbs checks group into families, one row of the picker each: Pressure (MSLP), Heights (levels), Thickness (1000-500, 1000-850, 850-700), Temperature (levels, 2 m), Dewpoint, Humidity, Wind speed (levels), Barbs (levels). A family has its own line style for every level (color, width, dashed, the interval, whether the numbers are written), set once with the swatch editor and shared by every preset and model, as the fill scales are. The levels of a family are chosen in the family's own row (chips: 850 700 500 ...), which replaces the level-by-field grid: 8 families by about 7 level chips, about 220 px. Each family says what it adds ("Heights: 700 green, 850 red") in the title and the legend as today. To keep a busy chart readable: a family draws its numbers on a spacing so two labels never sit on one another (the 1024/1025 pair and the height labels overlap in the busy chart), the barbs of a second and third level thin out to every other one of the first's, and a preset can mark a family "light" (thin lines, no numbers) so a background family does not compete with the one that matters.

**Choosing a preset sets the checks** (2026-11-13): picking a preset ticks exactly its fields, so the picker always shows what the chart has: the two fills (field, level, threshold), the family chips that are on, and the family styles in use. Nothing carries over from the chart before it (a preset is a whole state, not an addition); the name is the preset's until a check changes, then "Custom: <preset> + N changes". Clicking the same preset again resets to it. A preset the model cannot fully draw (a field the model lacks) ticks what it can and greys the chip it cannot, with the reason in the tooltip. The picker reads the chart on view the same way: opening it on any chart ticks that chart's own checks, so the grid is never out of step with the picture.



## 10. One model's field over another model's chart (idea, 2026-11-13; nothing written)

Asked while drawing §9: a SPC REFS product (a probability, a paintball, an ensemble max) drawn over an upper-air chart (500 mb heights and barbs) from another model. Out of the scope of §9, which keeps every picker inside one model, but §9 leaves the room for it: the picker's Fill row picks a field, the contours and barbs rows pick the families, and a "From model" choice next to the fill would say where the field comes from.

**Why it is not a small addition** (to check in the code before any work; these are the likely problems, not findings): today the ticked fills are fields of the same model and run as the base chart (`compose()` with the `o_<id>_` prefix), so they share one grid, one run and one valid time. A field from another model has its own grid (REFS vs GFS/ECMWF), its own run, and often a window ("4-h max") where the base chart is an instant. Open questions: which run of the second model matches the base chart's time (latest, or the one valid at that time); which time of a windowed field is the chart's (the window ending at the valid time); resampling the second field to the base grid, or drawing each in its own projection; the legend and hover naming the model of each layer; and which model's timeline the slider follows.

**The picker for the second field**: REFS products are a product plus up to three variant axes (window, statistic, threshold; 168 ids), so the second field is chosen with a variant picker (product dropdown, then window / statistic / threshold chips, combinations that do not exist struck through, the choice resolved to one chart id) rather than the field list. The same picker would replace the separate number and mode dropdowns of §9 wherever a product has those axes (WPC amounts, ensemble statistics, the two-limit RH-and-wind charts). Not yet checked: that every REFS id fits the three axes (the two-limit ids `t<015_t>020` need two threshold rows).

**Steps if taken up**: (1) parse the REFS ids into product, window, statistic and thresholds and count the ids that do not fit; (2) the variant picker on REFS alone, with no overlay; (3) the same-model overlay with REFS fields on a REFS chart; (4) the matching rule for a second model's run and time, tried on one pair (REFS over the GFS 500 mb chart).

**One-offs** (decided 2026-11-13): a chart that no family reads stays a one-off: it keeps its own entry in a "More charts" list (grouped by category, starrable as a favorite) and is not part of the presets. The families are Value, Precipitation (period), Chance, Amount, Ensemble, Anomaly and Change, with the overlay families; the check is `docs/presets/models-check.csv` (95% of the 2656 chart entries read, 56 of 76 models fully) and the charts that did not read are listed in `models-check-exceptions.csv`. So no family is stretched to fit them: the fixed-definition chances (calibrated severe, flight categories, thunderstorm), the two-limit chances (wind and RH, blizzard), the "other lines" (CAPE/CIN pairs, ship), the waves lines, the members contoured at a value and the ensemble charts whose statistic is not named stay as they are. If a group of one-offs grows (the fixed chances in SPCPOST and SPCGEFS are 18 charts), it can get its own family later; nothing has to be decided for it now.

**Fire weather, the one place with a second limit** (decided 2026-11-13): the 14 SPC REFS wind-and-RH chances (wind > 10 to 30 mph with RH < 10 to 35%) are one product with two linked limits, so the Chance family gets a second limit row for this group only: Wind > [10 15 20 25 30 mph], then RH < [only the values that exist for that wind: 10 mph 35; 15 mph 15 to 35; 20 mph 15 to 30; 25 mph 15 to 25; 30 mph 10], 14 of the 30 pairs. The rest of the group folds the same way: Fosberg (chance > 50 or > 75, ensemble max, mean, min) and 2 m RH (ensemble max, mean, min) are each a statistic dropdown, and the deterministic fire fields (Haines, PBL height, ventilation rate, transport wind, 1-hour minimum RH with 10 m wind) are plain Value charts. So SPC REFS fire weather is three presets in place of 22 charts. The other two-limit charts (blizzard, precipitable water between two values) stay one-offs.

**Where §9 stands (2026-11-13, after the checks of every model)** (data in `docs/presets/`; nothing written in the app):

- **The theory**: a chart is a field (the fill) plus overlays (contours, barbs, streamlines, type bands), and a product may carry variant axes: level or layer, period or hour, statistic or type, limit. A model offers a subset; the picker shows only the axes a product has and strikes through the values a model lacks. The picker is three rows: Field (grouped), the variant row(s), the overlays (contour chips, a barb row of levels, each with its color).
- **Families** (`families-proposed.csv`): Value (field at a level), Precipitation (period), Chance (product, hour, limit, kind; a second limit only for the fire weather wind and RH pairs), Amount (expected, percentile), Ensemble (members, spread; field, level, statistic), Anomaly, Change, and the SPC REFS product grid (product, hour, type, limit; `spcrefs-grid.csv`, `spcrefs-matrix.csv`: 15 products with an hour-by-type grid, about 12 with only an ensemble statistic, 28 single-field means that fold into field + level, 14 fire pairs, 6 one-offs). Check: 2539 of the 2656 chart entries (96%) read, 56 of 76 models fully (`models-check.csv`).
- **The rules**: fold a group only where its entries differ by a number or a statistic (level, hour, limit, percentile, member); never by field (Surface, Air quality and the instability fields stay a plain, searchable, collapsible list, starred as favorites). A chart no family reads stays a one-off (76 ids, `models-check-exceptions.csv`). Presets differ between models where the same id differs (27 ids).
- **Singles** (`singles.csv`): after the folding, 228 ids are left alone in their pattern or one-offs (152 plain single fields, 76 one-offs), from the 380 counted before the REFS grid and the fire pairs. Surface (44) and Air quality (24) are plain lists by choice.
- **The alias table** (`alias-table.csv`, 865 ids): each id read as a family and its settings (field, level, chance product / hour / limit / kind, contours, barbs, streamlines, bands). It works both ways (id to checks when a chart opens; checks to id when the picker lands on a chart), so saved views, favorites, the comparison tiles and the Extended hand-off keep their ids. A one-off is its own alias.
- **Still to decide**: layout A or B; the level as chips or a grid row (the barb row is chips in the sketch); whether a ticked fill replaces or overlays the chart's own fill; dewpoint and RH as two fill entries or one with a switch; the Members and fixed-chance families (left as one-offs for now); the per-model preset where an id differs.
- **Steps, as they now stand**: (1) the alias table in the code, with a test that every id round-trips (id to checks and back to the same id), no UI change; (2) the Preset record and save/load ("Save checks as preset..."); (3) the three-row picker on GFS (the standard) with the 17 presets of `gfs-presets.csv`; (4) the same on the other deterministic models, then the ensemble and probability models (Chance, Amount, Ensemble, the REFS grid); (5) the second fill, the families of contours and the palette swatches.

**Built so far, and the open points decided** (2026-11-13; the Checks... button of the Model Viewer, `src/models/ChecksPicker.*`, `src/gfs/ChartAlias.*`, `ChartPreset.*`; tests in `tests/gfs/run_alias.sh`, `run_preset.sh`, `run_checkspicker.sh`, `run_fillfrom.sh`, `run_linestyle.sh`): steps 1 to 3 are done and the picker sits beside the Charts... picker, which stays. It has: your presets and ten starters (`ChartPreset::starters`), a search box and a star (the favorites are the Charts... picker's own list, `MODELFAV_<model>`), the Chart list (a model's charts as 39 kinds on GFS, grouped by category), the Value (500mb, 6 hours, 1000-500mb), for a chance or an ensemble statistic the three lists Hour, Type and Limit (each offers what exists with the ones before it: SPC REFS, WPC), one dropdown for each family of contours, barbs and fills with its levels ticked in it, a line that says what is ticked, a colour strip beside each fill level (the Colors editor), a "from" value on each fill (clear under it, tinted from it up: the second, darker fill), and a style button on each line (colour, weight, dashes, interval, numbers). Decisions: (1) dewpoint and relative humidity stay two fill entries, not one with a switch: the humidity is the heavier to draw (the program works it out from the temperature and the dewpoint or the specific humidity) and each has its own scale and start; (2) a ticked fill overlays the chart's own fill, translucent, one at a time (the 'from' value is what keeps the two readable together), it does not replace it; (3) Surface, Air quality and the instability fields are plain charts of the Chart list, found by the search or the star, not folded. Still open: the 46 ids whose checks another id shares (merge the true duplicates or tell them apart), the checks picker on the other models (only GFS and SPC REFS were looked at), label spacing and a 'light' family, retiring the Charts... picker, and the cross-model overlay of section 10.

**The 46 ids that shared their checks, settled** (2026-11-13): they were three kinds. (1) Sixteen ids in eight pairs are different charts of one model that read the same (2 m maximum and minimum temperature, CAPE and CIN of the most unstable and of the surface parcel, the GEFS-MLP significant and any-severe probabilities, the bulk and the Richardson PBL height, the four simulated satellite channels): they now have a `detail` in the alias table that tells them apart. (2) Twenty-eight ids in fourteen pairs are the same chart under another id on another model (`2m_tmpc` and `temp_2m`, `ens_prob_total_0.25in` and `prob_total_0.25in`, `cape` and `sbcape`): not a fault, they are how a preset finds the chart on the other model; `ChartAlias::idFor(entry, model)` gives a model's own id for the same checks, and a preset put on a model uses it. (3) `10m_wnd` and `mslp_10m_wind` are one picture under two ids on the HRRR and RRFS family (MSLP from MSLMA or from PRMSL): the first in the model's list wins. One caveat: on the GEFS `10m_wnd` is the wind spread, a different chart (one of the 27 ids whose checks differ by model, which the table reads as the most common model's); the alias table has no per-model rows yet.

**Kinds, after looking at every model's list** (2026-11-13): charts that no family reads are grouped by the pattern of their id, so the spaghetti contours (GEFS 18, IFS-ENS 8) are two or three kinds with the contour values as the Value list; the WPC rain, snow and ice amounts and chances are one kind each (Hour, Type, Limit); a product kind holds the charts of all the categories it is in. `tests/gfs/run_models.sh` fails for reasons that are not these (checked 2026-11-13): it still asks every model for the old `why` and `links` fields, which became Markdown pages (`help/models`) on 5 Nov, it expects HRRR not to be a registered model, and it finds seven clone models with fewer than 10 charts (ARWEN, WOOF-NAM, ICON-D2, ARPEGE-EU, MSM, AROME-AT, MRMS) and AROME-HD with none.

**Labels with many families ticked** (2026-11-13, `e92fc20`, `4231634`, `d9ff161`): with a chart that has eight things ticked onto it the title ran into the run and valid times and the line under it lost its last barb level. Now the title counts the lines and barbs ("with 7 more and 2 m dewpoint (fill)": the fill stays named, it is what colors the map); the line under it is a key, each line and barb name in the color it is drawn in, in 11 px, down to 9 px, then cut with an ellipsis; the numbers on the lines are drawn after the barbs (a barb was over a number), each tries 0.4, 0.6, 0.3, 0.7, 0.2, 0.8 along its line for a place clear of other numbers and of the barb grid. Checked by eye on one busy GFS chart, not by a test (text is not in the golden pictures, below). Not done: the vector export and the map layer path (`vectorOut`), label spacing and a "light" family.


## 11. Taking `render()` apart, and one rule for where things are drawn (plan, 2026-11-13; steps 1 to 4 are done, 2026-11-14)

`GfsChart::render` is one function of 1208 lines (`src/gfs/GfsChart.cpp` 8207 to 9423, in a file of 9400): the fill, the ticked fills, the coastlines, the lines with their numbers and the highs and lows, the barbs, the point values, the title and the legend, with the state they share (the painter, the view, the scale, the font, the placed numbers) as locals. Where things go is decided in separate places in separate ways: the numbers on the lines have a `placed` list, the barbs a clear-space check (added 2026-11-13), the highs and lows and the point values their own, the title and the line under it shrink and cut by their own rules, and the draw order (lines, barbs, numbers on top) is fixed in the code.

**Step 1, done (`698e410`): golden pictures.** `Options::noLabels` makes `render` draw no text at all (every text goes through one helper), so a picture of the colors, lines and barbs does not change when the rules that place text do. `tests/gfs/golden_test.cpp` builds ten charts by hand from made-up grids (no network, no catalogue): a fill; fill with height lines and barbs; flat bands with split-color dashed lines; lines only; stacked barb sets; a ticked translucent fill; rain with snow and mixed bands; streamlines; a local area with frame marks and a marker; a picture at twice the size. They are compared with `tests/gfs/golden/*.png` (up to 0.02 % of the pixels may differ); `GOLDEN_UPDATE=1` writes them again, `tests/gfs/run_golden.sh` runs it. Run twice they are identical; a heavy line of 1.9 px instead of 1.6 failed seven of the ten. Not covered yet: the paintballs, the member sets, the standard-deviation and maximum variants, the pooled and multi-model charts, the exports and the map layer's paths (`vectorOut`, `fillOut`), and every piece of text.

**Next, in this order** (each its own commit, the golden test green after each): (2) add the missing kinds of chart to the golden set before their code is moved; (3) one `RenderState` that every stage receives, in place of the locals; (4) take the stages out one at a time into their own files (the fill, the ticked fills, the lines and numbers, the barbs, the point values, the title, the legend), the pure parts (the header text, the tinted scale, the label candidates) as plain functions with tests of their own; (5) the chart catalogue (most of the rest of the file) out of `GfsChart.cpp` into files by kind.

**Steps 2 to 4, done (2026-11-14).** The golden set is 15 pictures (paintball sets with a layer taken out, the SPC REFS key, standard deviations, a banded maximum, the places of point values added); `GOLDEN_LABELS=1` draws the text, to compare a move with the text on (save with `GOLDEN_UPDATE=1 GOLDEN_DIR=/tmp/x`, compare with `GOLDEN_DIR=/tmp/x`; the fonts differ between machines, so those pictures are never kept). `render()` is now 145 lines that call, in order: `prepare` (the user's scales, the derived grids, the override fill), `fillProbe` (the read-out and export fields), `paintFill`, `paintOverlays`, `drawCoastlines`, `drawStreamlines`, `drawContours` (returns the numbers to draw after the barbs), `drawBarbs`, `drawPointValues`, `drawTracksMarkerAndTicks`, `drawHeader`, `drawLegend`. The stages share `MapFrame` (before the painter) and `Frame` (after it), and a few have a state struct of their own (barbs, point values, header, legend); the code in each was moved, not changed, with the 15 pictures identical with the text off and on. Not seen by any golden picture: the tracks and wind radii and the export's vector output. The read-out fields (`fillProbe`) are checked by `tests/gfs/run_probe.sh` (the fields in the order they are read, the place of the map, the winds, the range of the fill, what a point reads in C and in F). **Left for the placer:** the barb grid is worked out twice (the numbers keep clear of the barbs in `drawContours`; `drawBarbs` draws them): one spec to pass to both; the state structs fold into `Frame`; `drawTracksMarkerAndTicks` splits in three; a stage with an obvious test of its own (the number placement, the key of the sets) gets one.

**GfsRender, and the other big file (2026-11-14).** `GfsRender::png` (487 lines) is what comes before the drawing: the run and the hour, the fetching, the composing of variants and overlays, the options, the call to `GfsChart::render`, the bare chart and the PNG. Its options are now `GfsRender::OptionPrefs::read()` (the settings, read once) and `GfsRender::chartOptions(prefs, sector, place, variant, ...)` (a plain function of them; `tests/gfs/run_options.sh`). The choice of the run and the hour is now two plain functions, `GfsRender::runForCycle` (the newest run stepped back to the cycle asked for) and `GfsRender::hourForValidTime` (a valid time to a forecast hour, an analysis being its own run), with `tests/gfs/run_runselect.sh`. The variants (the maximum, the change, the difference) are `applyVariant` (in `GfsRender.cpp`; it reads other runs and models, so no offline test yet: the pure parts, the maximum of grids and the scale of a change, could be split out and tested). `png` is 273 lines (from 487). Left in it: the sector and the area round a saved location, the fetching with its fallbacks (the 1 hour pieces, the previous cycle), the HAFS track, the bare chart and the PNG. The other monolith is `GfsChart.cpp` itself (9,700 lines), most of it now the chart catalogue (ids, labels, derivations, ramps): step 5. The wish is **a data file over definitions in code**: the catalogue's plain parts (id, label, group, level, ramp name, contour sets, barbs, the family of the checks) as a settings file read at start, the derivations that are real code (a wet-bulb, a lapse rate) staying in code and named from the file. The alias table (`ChartAliasData.cpp`, generated from `docs/presets/alias-table.csv`) is the first piece that already is this shape: it would be read from the file instead of compiled in.

**The catalogue split, begun (2026-11-14).** `GfsChart.cpp` went from 9,715 to 3,321 lines (the first pass, 5,909, was the families; the second the REFS, the NBM / RTMA / URMA, the model clones and the GFS charts themselves). The charts of each family of models are in a file of their own, `GfsProducts<Family>.cpp` (Geos, Sst, Copernicus, MfWave, Current, Wave, Hafs, Geps, Spc, Meso, OpenMeteo, Ppp, Cfs30, SpcRefs, Ens, EnsAll, Href, Gefs, Rrfs), each one function `<family>Products(std::vector<Product>& p, ...)` called from `products()`; what they share is in `GfsChartInternal.h` (`pick`, `want`, `speedOf`, `heights`, `pressure`, `vorticityOf`, the generated-id helpers) with `GfsChartRamps.cpp` (the 44 color scales), `GfsChartExceed.cpp` (the chance of an amount), `GfsChartIngredients.cpp` and `GfsChartCommon.cpp`. The build lists only one folder level, so all stay in `src/gfs/`. `tests/gfs/run_catalogue.sh` dumps every chart of every model from the built program and compares it with `docs/presets/charts-dump.txt` (5,312 lines: id, group, label, fill and line titles, the records and levels): a move that changes one line fails it (`save` after a change that is meant to). `products()` is now only the order of the models and what each calls (`gfsProducts`, `nbmProducts`, `cloneFrom`, the families); the GFS file is the longest (911 lines), with the helper lambdas it uses (`upper`, `thickness`, `startOf`, `totalNeeds` ...) still inside it. Then, with the file split done, the plain parts of a chart can go to the settings file (the wish above), one family at a time, the catalogue check as the proof.

**The rule, after that: one placer.** Everything that is drawn declares a kind, a priority, where it would like to go, a few places to try, and how much clear space it needs; one placer settles them. The higher priority keeps its place (a high or low over a number over a barb); a lower one tries its other places, then goes (a number), shrinks (a title) or shifts (a barb). The placer's result (the items and their boxes) is what the picture, the vector export and the tests read, so a new family says how much room it needs and writes no overlap code of its own, and the label test is a check on the list (no two numbers overlap, none in a barb's clear space unless its line has no free stretch, the title and the line under it fit). First for the numbers and the barbs only, the two that are hand-placed now, keeping the picture the same where nothing collides.

**The annotation rules as they are today (read from the code, 2026-11-14).** Nothing is placed by one rule: each kind has its own, and the order they are drawn in is the only priority.

| kind | where it goes | what it avoids | what it ignores |
|---|---|---|---|
| contour numbers (`drawContours`) | one per line longer than 150 px, at 50% along it; with barbs on, the first of 50, 40, 60, 30, 70, 20, 80% that is free | other contour numbers (their boxes in `placed`), the barb nodes (1.1 barb lengths), the frame (24 / 14 px in) | H and L, point values, the marker and track labels |
| highs and lows | at the extreme (radius 12, 8 or 6 by the view's width), the value 13 px under | only the frame (20 px in) | everything else, and everything after them avoids nothing of theirs |
| wind barbs (`drawBarbs`) | a grid of pixels (`BarbGrid::look`), the first set longest | nothing: the others avoid them | all labels; they are drawn over the H and L |
| point values | cities biggest first, then an even grid, 120 / 96 / 80 px apart | each other (their own `boxes`), the frame (4 px in) | the contour numbers, H and L, barbs: a translucent box is drawn over them |
| marker name, track labels | a fixed offset from the point (10, -8 and 9, -7) | nothing | everything |
| frame marks, header, legend | fixed places; the header and the line under it shrink, the legend's keys run left to right | the header shrinks to 11 / 9 px and is cut with an ellipsis; the key of the sets does not wrap | a key can run past the right edge (not checked) |

Found in it: (1) the width of a number's box is guessed (`text.size() * 8`), where the point values use the font's real metrics; (2) the barb grid *rule* is one (`BarbGrid::look` and `lonStep`, used by every map and chart: that part is right), but the walk over its points is written out in `drawBarbs`, `paintBarbs` and `paintStackedBarbs`, and the clearance of the numbers worked out its nearest node by hand: that is now `BarbGrid::nearestNode` (`tests/gfs/run_barbgrid.sh`); the walk over its points is now `BarbGrid::forEachNode` / `forEachWorldNode`, used by `drawBarbs`, the two loops of `ModelChartLayer` and the globe's in `WorldRender` (the test compares it, point for point, with the loops it replaced; the map layer and the globe have no picture test of their own, and `SarWindsLayer` walks a different, cell grid and is left); (3) in the map layer the numbers are vectors placed at the picture's size, so their clearance from the barbs is that of the picture's barb grid, while the layer draws its barbs for the zoom on view: the two grids can differ, so a number can end under a barb there although the picture is clean; (4) the inset from the frame is a different constant for each kind (24 / 14, 20, 4); (5) no kind knows about the H and L, so a number or a point value can sit on one. **Fixed so far (2026-11-14):** the numbers on the lines and the point values now keep clear of the highs and lows, and the point values of the numbers on the lines (`drawContours` returns the boxes it placed, the point values start from them); `Options::placedOut` hands out every reserved box with its kind, and `tests/gfs/run_placement.sh` checks that no two overlap and each is inside its frame margin, on three widths and a dense chart, and on points put on the very places of the numbers (the test fails with either change taken out). The box of a number is now the width of its text as drawn (the font's metrics, not 8 px a character), and the highs and lows of every set are registered before any line is numbered (a height number was landing on a pressure high when the heights came first). The name of the marker (the ring on an area round a saved location) takes the first free side (right and above, right and below, then left) and records its box (the test fails with only the first side). Still open: the barbs are not obstacles for the point values, the track labels place nothing, and the map layer's vector numbers are placed at the picture's size.

**Related: the maximum (and minimum) point value of a field (2026-11-14). First part built: a "Mark the maximum" toggle in the Map menu (pref `MODEL_MAX_MARK`, off by default, like "Mark the place") draws a ring and the number at the place of the largest value the fill shows, the one the legend's "max" tick gives (`drawMaxMark`, `FillResult::maxAt`; a golden picture `max_mark`). Not yet: the minimum, the local extremes for any field, the placer's avoidance of the barbs and numbers (the number only flips side near the frame), the map layer (the mark is in the picture only).** Today only a contour set with `highsAndLows` (the pressure, one height overlay: three places in the catalogue) marks its extremes, as H and L with the value under. The legend already marks the largest and smallest value on view (the red tick; `fillMax`, `fillMin`) but not where they are. The same annotation for any field (a temperature, a pressure, a height, a dewpoint) is one kind with two scopes: *local* (every extreme, as H and L now: "H 1032") and *the one* (the single largest and smallest on view, a small marker and the number: "104", "-12"). It belongs in the placer as a kind of its own, with the barbs as obstacles and its priority above the contour numbers (it is the thing read first); the number is in the user's units (`shown(quantity, v, us)`, as the point values do). It needs: the location of the extreme of the fill (`paintFill` keeps only the value: it would keep the pixel too), a per-chart or per-tick option (`Product::extremes`: none, local, one), the existing `GfsGrid::extremes` for the local scope, and, for the map layer, the same two labels as vector labels (they exist for H and L already). The picker side is a tick on the checks (a family "Marks": "max/min value", "highs and lows"), saved with a preset like the other ticks.

**What the placer would take over:** a list of things that want a place (kind, priority, the places to try, the box from real metrics), the barbs first as fixed obstacles (the grid in one function both the picture and the layer call), then the H and L, then the contour numbers, then the point values and the labels, each avoiding what is already placed; the same function for the picture and the map layer's vectors (placed again for the view on screen). The test for it is a picture with the text on and a crowd of everything (a line through every barb node, an H on a line, points of all kinds), checked not by pixels but by the boxes the placer returns: none overlaps another, every one inside the frame.

**Machine use, a thought for the same stretch:** the program is driven today by a number of separate `WXQT_*` variables that work only in odd places (`WXQT_DRAW` only inside the `magcheck:` route, `WXQT_CLICK` meaning two things). One documented command line (`render <model> <chart> <hour> --overlays ...`, `dump charts`) with fixed, JSON outputs, and one runner for `tests/gfs/*` that shows the stale ones (`run_models.sh`: it asks for the old `why` and `links` fields, expects HRRR not to be a model, and finds seven small clones and AROME-HD with fewer than 10 charts or none), would make the tests, the CSV tools of section 9 and any script read the same thing. Not decided.

## 12. Android build (APK) and GDAL - notes (2026-10-30)

_(Was `docs/%s`, folded in here on 2026-11-14.)_

Status: **on hold, nothing implemented.** Written down so the question stops costing thinking time.
Related: "Separate minimalist Android / iOS app for SPC REFS" in `outstanding-work.md` (scoping archived in `~/Claude/spc-refs-app-docs.zip`).

### Decision so far
- **Keep GDAL as subprocesses** (`gdalwarp`, `gdal_translate`, `gdalinfo`). They give parallelism (one process per warp, no GDAL global
  state, no mutex), cancel/timeout (`QProcess::kill()`), and isolation (a crash or out-of-memory fails one chart, not the app).
  Embedding libgdal would lose all three and means rewriting ~31 call sites in 14 files. **Not planned.**
- The runners are not one shared helper (`runProcess` / `runGdal` in `UtilityGrib.cpp`, a `runProcess` in `UtilityRefs.cpp`, raw
  `QProcess` in `GfsData.cpp`, `GfsRender.cpp` and two spots in `UtilityGrib.cpp`). A tidy-up at most, not a reason to refactor.

### What stops a straight Android build
1. GDAL tools: Android cannot exec a binary from app data (Android 10+).
2. The widgets UI is desktop-shaped (not touch friendly).
Radar, text products and the SPC pages need no GDAL; the Model viewer / warped GRIB charts do.

### Route to try if the tools are wanted on Android (smallest change, untested)
- Build the GDAL tools for `arm64-android` with vcpkg (same manifest as `.github/vcpkg/vcpkg.json`, triplet `arm64-android`; netcdf-c / HDF5
  is the likely trouble spot), rename each to `lib<name>.so` and put them in `lib/arm64-v8a/` of the APK.
- Android lets an app run a native library from `nativeLibraryDir`, so `QProcess` can start it: arguments, threading and kill stay as today.
- Needs: a resolver for the tools on Android (today `QStandardPaths::findExecutable`), `proj.db` + `GDAL_DATA` copied out of the APK assets on
  first run with `PROJ_DATA` / `GDAL_DATA` set, a CA bundle (`CURL_CA_BUNDLE`) for `/vsicurl/`.
- **First test on a real phone, before building more:** package only `gdalinfo` and run it through `QProcess`. Some devices / configurations may block it.

### Fallbacks if that fails
- Embed libgdal (static or shared) behind a compile flag, desktop unchanged. Cost: serialise GDAL use, no cancel, size +20-40 MB per ABI.
- Small in-process GRIB2 decoder + resample (needs libaec for the CCSDS-packed IFS / AIFS). No GDAL on Android, but a decoder to write.
- Or sidestep: the separate minimal SPC REFS app (no GDAL needed).

### Workflow sketch (`.github/workflows/android.yml`, manual like `build.yml`)
`actions/setup-java` (temurin 17) -> `android-actions/setup-android` (platform 34, build-tools 34.0.0, NDK r26b `26.1.10909125`) ->
`jurplel/install-qt-action@v4` twice (desktop `linux_gcc_64` for moc/rcc, then `target: android`, `arch: android_arm64_v8a`, Qt 6.8.3,
`extra: --autodesktop`) -> `qmake ../wxqt.pro CONFIG+=release` -> `make` -> `make apk_install_target` -> `androiddeployqt --gradle`
(`--sign` with a keystore from repo secrets, or `--debug` for a sideloadable APK) -> `actions/upload-artifact`.
Also needed: an `android { }` block in `wxqt.pro` (package name, version code, manifest, excluding GDAL-dependent code). Play Store wants an AAB (`--aab`).
Only `arm64-v8a` at first; each extra ABI multiplies the build.

## 13. Derived severe-weather indices (SHIP, SIGTOR/STP) — plan

_(Was `docs/%s`, folded in here on 2026-11-14.)_

Status: **planning**, 2026-09-12. Continues a conversation the user had
elsewhere that produced a working Python/MetPy/xarray prototype for **SHIP**
(Significant Hail Parameter) computed from RRFS fields, dual-contoured with
native HAILCAST max hail diameter. Goal: port the computation into wxqt as a
new derived product, the same way "2m Temp / Dewpoint" and "2m Temp / Dew /
Wind" are already derived products in `UtilityGrib` - but SHIP needs ~9 input
fields and real thermodynamic math, not 2 fields and a straight fill+contour,
so it needs its own small piece of architecture, not just a new `Field` row.

### SHIP formula (confirmed against the Python prototype + SPC's mesoanalysis definition)

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

### Field mapping — verified against a live RRFS run (2026-09-12, F003, 00z)

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

### Architecture: this doesn't fit the `Field` table - it's its own render path

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

### SIGTOR / STP

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

### Staged milestones (targeted segments, per the user's standing instruction)

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

## 14. GFS charts: what is missing, and how anomalies could work

_(Was `docs/%s`, folded in here on 2026-11-14.)_

### What Tropical Tidbits' GFS menu has that the app does not (as of 2026-10-08)

Taken from the menu at tropicaltidbits.com/analysis/models/?model=gfs.

| Group | Missing | Needs | Effort |
|---|---|---|---|
| Precip / moisture | Rain / frozen split (MSLP & precip, radar) | `CRAIN CSNOW CFRZR CICEP` flags (in the idx) | small: tint the fill by type |
| | Snowfall 10:1, 24 h snowfall | `APCP` x flags, or `WEASD` | small |
| | 24 h positive snow-depth change | `SNOD` at two hours | small (the change chart exists) |
| | 700-300 mb mean RH, 2 m dewpoint | `RH` at 700 / 500 / 300; `DPT` | trivial |
| | MSLP & PWAT | have both | trivial |
| | PWAT normalized anomaly | climatology (below) | needs climatology |
| Lower dynamics | 700 / 850 mb temperature advection and frontogenesis | `TMP UGRD VGRD`, horizontal gradients | medium: gradient helpers beside vorticity |
| | 850 mb Okubo-Weiss, dilatation axes | deformation from `u v` | medium |
| | MSLP 48 h forecast trend | two hours of `PRMSL` | small |
| | MSLP anomaly / normalized anomaly | climatology | needs climatology |
| Upper dynamics | 2 PVU pressure and wind | potential vorticity from the pressure levels | hard |
| | 330 K PV; 300 K / 290 K wind, RH, pressure | isentropic interpolation of the levels | hard (same machinery as 2 PVU) |
| | 200-400 mb Q-divergence, streamfunction, 850 vort, 200 irrotational wind | Q vectors; Helmholtz split | medium / hard |
| | Z500 48 h trend, 500 mb height anomaly | two hours; climatology | small / needs climatology |
| | Simulated IR satellite | `ULWRF`, or cloud-top from `TCDC`; a brightness-temperature curve | medium |
| Thermodynamics | SBCAPE and wind crossovers | `CAPE` surface; 10 m and 850 mb winds | small |
| | 700 mb temperature, wind, MSLP | have the records | trivial |
| | 850 mb temperature anomaly; 2 m temperature anomaly | climatology | needs climatology |

Not on their menu but in our tropical set: deep-layer shear, steering flow, 200 mb divergence, 850 mb theta-e, streamlines (done).

### Climatology (anomalies and normalized anomalies)

GFS files carry no climatology, so an anomaly chart needs a reference dataset.

**Candidate: NCEP/NCAR Reanalysis 1 daily long-term means, 1991-2020** (NOAA PSL, open, no account):
`downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/pressure/` (`hgt.day.ltm.1991-2020.nc`, `air.day.ltm.1991-2020.nc`, `rhum`, `shum`, `uwnd`, `vwnd`; about 250 MB each, 17 levels), `.../surface/` (`air.sig995.day.ltm.1991-2020.nc`, `pres`, `slp`, `pr_wtr.eatm`). The `4Xday` versions are six-hourly (1 GB each); not needed.

Why it is workable even though the files are big: it is a 2.5 degree grid (144 x 73) with one value per calendar day, so one field at one level for the whole year is 365 x 10,512 values, about 7.7 MB as 16-bit numbers and 1-2 MB compressed.

**Plan: fetch only the slices a chart needs, keep them forever** (checked 2026-10-08)

The long-term-mean files are NetCDF-4 (HDF5, deflate), 15-260 MB each, but every file stores each day (and level) as its own chunk of 144 x 73 values, and the server honors HTTP range requests. GDAL reads it through `/vsicurl/` and downloads only the chunk asked for: one band took about a second and gave exactly 10,512 values (42 KB raw, a few KB on the wire).

- Surface fields (`slp`, `pr_wtr.eatm`, `air.sig995`): one band per day, `band = day + 1` (day 0 = 1 Jan; there is no 29 Feb: it uses 28 Feb).
- Pressure-level fields (`hgt`, `air`, `rhum`, ...): level varies fastest, `band = day * 17 + levelIndex + 1`, levels 1000, 925, 850, 700, 600, 500, 400, 300, 250, 200, 150, 100, 70, 50, 30, 20, 10 hPa (verified: band 1 is 1000 mb, band 2 is 925 mb).
- Command shape: `gdal_translate -q -b <band> -of ENVI -ot Float32 "NETCDF:/vsicurl/<url>:<variable>" out.raw`, the same ENVI decode `GfsData` already does for GRIB.
- **Cache:** one small file per (field, level, anchor day) in `PermanentCache("climate")`, compressed, about 15 KB. Anchor days are every fourth day (days 0, 4, 8, ...): the long-term mean is a smooth seasonal curve, and interpolating linearly between anchors costs less than the 2.5 degree grid's own error. A field then never needs more than 92 fetches in its life, and a ten-day forecast chart needs at most four. After that the chart works offline.
- `GfsClimate::at(field, level, validDay)` returns the two bracketing anchors (from the cache, or fetched in parallel and then cached), blends them in time, then bilinearly onto the GFS grid.
- Normalized anomalies: the standard deviation is only published monthly, as a handful of small files (`*.mon.inter.std`, under 10 MB each): fetch the one needed month the same way and cache it (the whole 12-month grid for one field and level is a few hundred KB).
- The libpq / libarrow errors GDAL prints on this machine are missing optional drivers and are harmless; suppress or ignore stderr from the call and judge by the exit code.

**Risks and honest limits**
- Reanalysis 1 has known biases against the newer reanalyses (ERA5, CFSR) and is coarse. For 500 mb height and MSLP it is fine; for 2 m temperature over mountains it is poor (the 2.5 degree surface is far from the real terrain). For T2m anomalies ERA5 0.25 degree 1991-2020 would be better but needs a Copernicus account and a heavy download; do not start there.
- Normalized-anomaly colors depend on the standard deviation field, so test them over the winter midlatitudes where spread is large.
- 1991-2020 is the current WMO normal; the older 1981-2010 files exist too.

**Order of work if you want it:** MSLP anomaly and Z500 anomaly first (large-scale fields, least affected by the coarse grid), then PWAT, then 850 mb temperature, then T2m last.

## 15. History: an archive tier and one fetch path (plan, outline)

_(Was `docs/%s`, folded in here on 2026-11-14.)_

Raised 2026-10-15 ("history is something I think about; relates to the cache as research / redownloading the same files").
Status: **plan only**; nothing here is built except the pieces listed under "Already in place".

### 1. Why

- The caches are built to forget. Yesterday's model run is gone after 48 hours; several caches are in `/tmp` and clear on reboot or when a newer run appears.
- Research needs the opposite: reopen a run, a loop or a day weeks later, offline, without downloading it again, and keep what belongs to one event together.
- The same file is fetched again and again today (a station's BUFKIT file, the same MRMS scan for two screens, GIBS tiles on every start).

### 2. Today's caches (measured 2026-10-15)

| Cache | Where | Size | Rule |
|---|---|---|---|
| Model slices (`GfsCache`) | `~/.cache/wxqt/gfs-v3` | 778 MB | pruned at start: older than `MODEL_CACHE_HOURS` (48) or over `MODEL_CACHE_MB` (2048), oldest first |
| GRIB slices, rendered charts, BUFKIT zlib | `/tmp/wxqt_grib` | 400 MB | `purgeCacheForOldRun`: removes files not of the run just resolved; BUFKIT: older runs |
| MRMS decoded scans | `/tmp/wxqt_mrms` | small | dropped after a few hours when a scan list is read |
| SPC Post, CAMS JSON | `/tmp/wxqt_spcpost`, `~/.cache/wxqt/cams` | small | by run / by age |
| "Permanent" data (`PermanentCache`) | `~/.local/share/wxqt/data/{tornado,hurricane,drought,climate,recon}` | 52 MB | newest version kept, older removed |
| GIBS tiles | memory | 0 | gone at close |

### 3. The design

#### 3.1 One fetch path (the part that stops redownloading)
`DataStore::get(key, fetcher, policy)`; order: **archive -> rotating cache -> network**.
- `key`: source, product, run or valid time, record or tile address (stable, human readable).
- `policy`: how long a rotating copy lives, whether a changed upstream file (ETag / size) replaces it, compression.
- Every downloader in the app (GRIB slices, MRMS scans, GIBS tiles, BUFKIT, SPC, CAMS) calls it instead of doing its own file handling.

#### 3.2 Archive tier (kept, not rotated)
- Folder `~/.local/share/wxqt/archive/<source>/<product>/<run or day>/...`, raw bytes as downloaded (GRIB slices with their `.idx` info, BUFKIT text, MRMS grib2.gz, GIBS tiles), compressed where it pays.
- **Keep** actions on the screens: "Keep this run" (model, area, products fetched), "Keep these scans" (MRMS loop), "Keep this day" (GIBS tiles of the view), "Keep this sounding".
- Never trimmed by the rotating rules. Own size cap for *unpinned archive* (least recently used first) and a visible total.

#### 3.3 Cases
A named bundle (`2026-10-15 Oklahoma storms`): the runs, scans, tiles, pictures and loops kept for one event, with a note.
Opens as one folder; screens can switch to "case mode" and read only from it (offline).

#### 3.4 Index and viewer
- Small index (SQLite or a JSON manifest): source, product, run / valid time, size, fetched at, pinned, case.
- **Data kept** screen: usage by source, keep / delete / open, case list, the cache settings in one place.

#### 3.5 Retention
- Rotating: as now (hours, size), applied by the one store, same setting names.
- Archive: size cap for unpinned items only; pinned and case items are never removed by the program.
- All deletions go through the one store (no per-module `QFile::remove` of cache files).

### 4. Work packages (in order)

1. **Store + migration of the temp folders**: `DataStore` with the rotating tier only; move `/tmp/wxqt_*` into the cache directory (survives reboot); same hours / MB settings.
2. **Move the downloaders onto it**: GRIB slices (`GfsData` / `UtilityGrib`), MRMS, BUFKIT, SPC Post, CAMS, GIBS tiles.
3. **Archive tier + Keep buttons** on the model screen (run), MRMS, satellite (day), soundings.
4. **Index + Data kept screen**, then **cases**.
5. **Offline / case mode** on the screens (read only from the store; say what is missing).

### 5. Decisions needed

- Scope: keep on request only (cases), or also automatic rules (every run with a named storm / alert, the last N days)?
- Size budget and location of the archive (default 20 GB in the home folder? a user-chosen path?).
- Granularity of a case: date range + area, or the list of things pinned?
- Derived products (rendered charts, loops): stored with a case, or always redrawn from the raw data?
- Privacy / terms: GIBS, NOAA, Sentinel data are open; EUMETSAT / Copernicus items have attribution requirements: store the credit line with the item.

### 6. Already in place

- `GfsCache` age + size pruning with the two settings.
- `PermanentCache` newest-version policy for the small data files.
- BUFKIT files kept compressed per station and run (rotating tier; dropped when a newer run is read).
- Compressed storage helpers (`qCompress`), run id naming in file names.

### 7. Revision 2026-10-15 (after review): start from an event, on a history page

Critique of sections 3-4 above (they stay as the storage ideas, in a lower place):
- Most sources keep their own open history: MRMS on AWS back to at least 2020-10, HRRR from 2014, GFS from 2021, GIBS by date for years, SPC outlook archive back to 2010 (checked 2026-10-15), IEM (Iowa Mesonet) for ASOS / VTEC / LSR, NCEI / ACIS for daily climate (Springfield MO back to 1940). RRFS is only about two months. History is therefore mostly **a time to ask for**; local archive is only for what expires (RRFS, BUFKIT, CAMs, SPC REFS, live obs) and for the person's own saved pictures.
- Screens are built around "latest". The cross-cutting work is **"open at time"** for each screen (route + parameters), not storage.
- One small index (SQLite) rather than sidecar JSON per file; a dated folder path is for human browsing only.

#### 7.1 Entry: an Event
`Event { id, name, kind, start, end, area (point + radius or box), source, note }`
- **From a catalog**: tornado outbreaks and tornado days (the tornado list), hurricanes and storms (HURDAT / the storm list), SPC high-risk days, big winter storms.
- **From the person**: "the January storm that gave us 12 inches": a date or range, a place (a saved location), a name.
- **Found, not remembered**: *Find the storm* at a saved location, from daily climate (ACIS `StnData`, element `snow`, `pcpn`, `maxt`, `mint`, wind gust): the biggest days and the biggest 1 - 3 day totals in a range, to pick from. (Springfield MO, snow: 2000-12-13 13.7 in, 2011-02-01 10.0 in, 2002-12-23 7.8 in ... in the daily record.)

#### 7.2 The history page
- Top: the event (name, dates, place), a time cursor inside it, and what is open upstream for it.
- Body: a grid, one cell per data type with data for that time and place: radar (MRMS / NEXRAD), satellite (GIBS), model run nearest, SPC outlooks / watches / mesoscale discussions, storm reports (LSR), warnings (VTEC), surface obs (ASOS), upper air (SPC soundings), daily climate (ACIS), surface analysis maps, tornado / hurricane tracks.
- Each cell says *where it comes from* (open archive / kept here) and *how complete*; a click opens the screen **at that time** with the event's area; the time and area stay on a small bar across the screens opened from it.
- Events are saved (index). An event can also hold the person's own pictures and notes (the "kept here" part).

#### 7.3 Order
1. "Open at time" routes for: MRMS scan, GIBS day, SPC outlook (day, issuance), observed sounding (site, time), storm reports (date), warnings (date), model run + hour (GFS / HRRR), daily climate (ACIS).
2. Event record + the history page over those routes; the retention table per source.
3. Find the storm (ACIS) and the catalog entry points (tornado, hurricane lists).
4. Local archive of the expiring sources, written when viewed, indexed; shown as "kept here".

### 8. Entry: catalog and "enter dates here" (decisions 2026-10-15)

- **Outbreaks last days**: a catalog outbreak is a connected group (tornadoes with gaps under about 12 hours, near each other), not a calendar day; start and end span days. The event keeps `catalogId` so it can be re-read; the person may widen / narrow it.
- **Any point**: the place of an event is any point on the map (a point picker, not only saved locations) with a radius; a saved location is just a shortcut.
- **Moments**: a list of (time, label) notes inside the event (nice to have); entered as lines "yyyy-MM-dd HH:mm label".
- **Default name**: datetime, latitude, longitude: `2026-01-14 12:00Z  37.24N 93.39W`.
- Built (first slice, 2026-10-15): `src/history/` - `HistoryEvent` (JSON store `~/.local/share/wxqt/history/events.json`), `PointPicker`, `EventsViewer` (list, entry form, "what there is for this time and place" with Open for SPC storm reports (by date), SPC outlook archive page, GIBS day (Terra MODIS), Iowa Mesonet, xmACIS). Toolbar: "History: events"; dev route `WXQT_OPEN=events`.
- Not yet: catalog entry (tornado outbreaks, storms), "find the storm" (daily climate), time cursor carried across screens, "open at time" for MRMS / models / soundings.

### 9. GOES history (idea 2026-10-15; checked the same day)

Sources for a GOES picture at a past time:
- **Iowa Mesonet GIS archive** `https://mesonet.agron.iastate.edu/archive/data/YYYY/MM/DD/GIS/sat/`: CONUS composite GeoTIFFs every 15 minutes: `conus_goes_ir4km_HHMM.tif`, `conus_goes_vis4km_HHMM.tif`, `conus_goes_wv4km_HHMM.tif` (about 1.3 MB each, 96 a day per product); listed for 2024, 2015 and 2010 (2019 listed far fewer files: check). 4 km, so the picture of a day, not of a storm's structure. The easiest "open at time".
- **NOAA open data on AWS** `noaa-goes16` (2017-), `noaa-goes17` / `noaa-goes18` / `noaa-goes19`: every ABI product as NetCDF: `ABI-L1b-RadC/F/M`, `ABI-L2-CMIPC/F/M` (one file per band; band 2 CONUS about 63 MB, 5-minute), `ABI-L2-MCMIPC` (all bands, about 53 MB), and the L2 products (cloud top, fire, lightning ...). Full resolution (0.5 - 2 km), 1-minute mesoscale. Heavy; needs the geostationary projection handled by GDAL.
- **NASA GIBS** `GOES-East_ABI_*` / `GOES-West_ABI_*` layers (GeoColor, Air Mass, Band 2, Band 13, Dust, Fire Temperature) have a 10-minute time dimension but the capabilities list only a window of weeks (not a history).
- The STAR CDN used by the GOES screen holds recent days only.

Proposed: a master-map layer `satellite/goes-history` (IEM 4 km, channel and time from the time bar / the event's "as of"); full resolution from the AWS NetCDF later, for an area and an hour at a time.

## 16. REFS Ensemble Viewer — plan

_(Was `docs/%s`, folded in here on 2026-11-14.)_

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

### What's actually new here (vs. just "point GribViewer at REFS files")

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

### Layout: four-panel comparison

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

### Export formats: full-res JPEG XL / animated JXL

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

### Data layer — RESOLVED 2026-09-14, from the authoritative source

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

### Architecture — reuse, don't rebuild

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

### Paintball rendering — mechanics

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

### Point graph — mechanics

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

### Staged milestones

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
- **Stage 1 - partly done (2026-09-30)**: `mean`/`sprd`/`pmmn` rounded out
  with 7 `Field` rows total (up from Stage 0's 2), each product type's
  *actual* field list verified live rather than assumed uniform across
  products - a real, useful finding:
  - `mean`: broad general fields (confirmed CAPE/TMP/WIND all present) -
    still no REFC.
  - `sprd`: the same broad set as `mean`, **plus REFC** (spread of
    reflectivity IS produced, even without a plain mean to go with it).
  - `pmmn`: **REFC only** - confirmed nothing else lives in this file.
  - `lpmm`/`avrg`: confirmed **precipitation-only** (3 accumulation
    windows apiece, sharing the same `:APCP:surface:` idx prefix
    disambiguated only by the window text) - not yet added, needs the
    same kind of threshold/window disambiguation Stage 4's probability
    picker will need anyway (a plain substring match can't tell "0-6 hour
    acc" from "3-6 hour acc" without a bit more care), so deferred to land
    alongside that rather than half-solved here.
  - `prob`/`eas`/`ffri` - **not started**, genuinely needs Stage 4's
    threshold-picker UI (prob's records share idx prefixes across
    different thresholds for the same variable - e.g. `REFC>10/20/30/40/50`
    all differ only in a `:prob >N:` suffix after a forecast-hour-length
    string that varies per render, so a single fixed substring can't
    select "just the 40 dBZ one" the way a plain `mean`/`sprd` field's
    single-record file can).

  New spread-specific colormaps (`tempSpreadColorMap`/`capeSpreadColorMap`/
  `reflSpreadColorMap`, `UtilityRefs.cpp`'s own anon namespace) rather than
  reusing the value colormaps for spread fields - a high-spread area
  reusing e.g. `capeColorMap` would render identically to a genuinely
  high-CAPE one, which is actively misleading for "how much do members
  disagree" rather than "what's the value." Also promoted `UtilityGrib::
  windColorMap`/`capeColorMap` from private to public (same treatment
  `tempColorMap`/`reflColorMap` got in Stage 0) so the new mean-wind/
  mean-CAPE rows reuse them instead of a second copy.

  Verified live against a real run (`2026093000`, F06, CONUS): all 7
  fields render valid RGBA PNGs with physically sane ranges, and the three
  spread fields' observed ranges (temp 0.02-2.9°C, CAPE 0-1177 J/kg,
  reflectivity 1.4-26.1 dBZ) sit comfortably inside their hand-picked
  colormap ranges (0-6°C / 0-1800 J/kg / 0-28 dBZ) - a real calibration
  check, not just "does it render." `RefsViewer`'s default 4-panel layout
  updated to show mean-temp/mean-reflectivity(pmmn) on top with their
  spread counterparts underneath, instead of two fields each shown twice.
- **Stage 2 — partly done (2026-09-30)**: individual RRFS Ensemble members
  are selectable as panel fields: 5 members x 6 fields (composite
  reflectivity, 2m temp, surface CAPE, max 10m wind, gust, 2-5km updraft
  helicity) = 30 new `UtilityRefs::fields` rows ("RRFS Ens Member N ..."),
  generated by `buildFields()`; a row's `product` slot holds `m00N` and
  `isMemberProduct()` switches the URL to the `rrfsens.*` tree (3-digit
  forecast hour, `2dfldnomads` file). Reuses the per-field idx byte-range
  fetch + per-field grib cache, so no separate parallel downloader was
  needed. `UtilityGrib::uphlColorMap` promoted to public for the UH rows.
  Verified live (run 2026093000, F06, CONUS): all 30 render, values sane,
  members differ. **Still open from Stage 2**: the plan's dedicated
  "Member N" product combo (currently members are just long-list field
  entries), and the linked-crosshair read-out (panels still have no
  `.grid` sampling).
- **Stage 2 (original text)** — member fetch/cache plumbing: parallel byte-range download of
  all members for one field+hour (`DownloadParallelBytes`, RTMA-style),
  cached per run+field+hour+member like `g_*.grib2` today. Panels can now
  show "Member N"; linked crosshair becomes actually interesting (four
  different members' read-outs side by side).
- **Stage 3 — done (2026-09-30)**: `UtilityRefs::renderPaintball` + 7
  "Paintball ..." field rows (REFC >= 20/40 dBZ, UH 2-5km >= 25/75, CAPE
  >= 1000/2500, gust >= 50 kt; thresholds are fixed per row for now - a
  free-form picker is Stage 4). Per member: idx byte-range fetch (shared
  `fetchFieldGrib`, factored out of `render()`), warp, then a 3-stop
  gdaldem table (alpha 0 below the threshold, member colour at alpha 110 at
  or above) - Python-free. Members composited with QPainter, CWA/state
  lines burned afterwards, legend (threshold + a swatch per member that
  actually rendered) drawn last. Missing members are skipped, not fatal.
  Verified live (2026093000 F06 CONUS): all 7 render; CAPE >= 1000 visibly
  shows the members stacking over the Gulf. No hover `.grid` sidecar yet
  (the natural one is members-exceeding count, which is also Stage 4's
  from-members probability). Cache key `pb1`.
- **Stage 4 — done (2026-09-30)**: runtime threshold + both probability
  sources. `UtilityRefs::fields` now has three row kinds per variable, driven
  by one `thresholdSpecs` table: **Paintball** (Stage 3, threshold now a
  per-panel runtime value), **Member Probability** (percent of available
  RRFS Ensemble members >= threshold, pointwise, via the shared
  `UtilityGrib::calcRaster`; goes through the same `finishRender` so it has
  a hover sidecar = percent) and **REFS Probability** (REFS's own `prob`
  product; `UtilityGrib::idxByteRange` gained an `alsoContains` argument to
  pick one ":prob >N:" record; a requested threshold snaps to the nearest
  published band and the status line shows the one used). Variables: refc,
  uphl25, cape (members: surface CAPE; REFS: mixed-layer 90-0mb - different
  quantities, labelled as such), gust (members only), 1km reflectivity and
  0-3km helicity (REFS only). Each panel has a threshold box (visible only
  for these rows, reset to the row's default on field change, Enter to
  apply). Legends: swatch key for paintball; 0-100 % bar for the
  probabilities. **Not the same thing**: verified live that REFS `prob` is a
  *neighborhood* probability (smooth, high) while the 5-member number is
  pointwise (small isolated cores) - both legends say so. Not added: precip
  accumulation `prob` records (window text varies with forecast hour),
  `eas`/`ffri`, lpmm/avrg.
- **Stage 4 follow-ups (2026-09-30, local)**: REFS Probability now also has
  1-hr / 3-hr / total precipitation, 3-hr / total snowfall (shown in inches,
  published in metres) and total freezing rain. Their idx records differ by
  a forecast-hour-dependent window ("5-6 hour acc" etc.), so
  `ThresholdSpec::probWindow` builds the window text from the hour. Real
  finding: the 3-hr and since-start windows exist only at forecast hours
  divisible by 3 (hourly windows every hour) - other hours say so in the
  status line. Paintball panels now have a hover sidecar (members
  exceeding, "N of 5 members").
- **Stage 4 (original text)** — probability-of-exceedance UI: threshold picker; direct render
  from `prob`/`eas` bands where available, from-members fallback where not.
- **Stage 5 — done (2026-09-30)**: `RefsPointGraph` (+ hand-drawn
  `PlumeCanvas`, no Qt Charts). Click any map (new `ZoomImage::clicked`
  signal, only fires when the press did not become a drag) to pick a point -
  a linked crosshair marks it in all four panels - then "Plume graph" opens
  the chart for the field in the clicked panel: one line per RRFS Ensemble
  member, a bold ensemble mean and (for Paintball / Member Probability
  fields) a dashed threshold line. Horizon combo 12-60 h (default 24). Hours
  are fetched one at a time off the UI thread (`UtilityRefs::
  memberPointValues`: per-member cached idx byte-range slice + `gdallocationinfo
  -wgs84`), so the chart fills in live; ~6 s per uncached hour (5 members),
  near-instant when the slices are already cached; closing the window or
  changing the horizon cancels the chain. REFS-only rows (mean/spread/pmmn/
  REFS prob) have no per-member data and say so. Verified through the real
  GUI (offscreen): click, button, 12 h chart rendered. Also added
  `gdal_contour`, `ogr2ogr` and `gdallocationinfo` to both packaging scripts
  - the first two were being called (contours) but never bundled.
- **Stage 5 (original text)** — `RefsPointGraph` (the third window): click-to-sample, plume
  chart.
- **Stage 6 — mostly done (2026-09-30)**: `RefsViewer::onSave` builds a
  single 2x2 mosaic (header line = run/valid time, caption strip per panel
  naming its field + threshold) and saves it via `UtilityJxl` (JXL, PNG
  fallback); with a loaded animation it builds one mosaic per hour ->
  `UtilityApng::fromFrames` -> animated JXL. Also done: **linked hover
  read-out** - hovering any panel snaps a crosshair in all four and each
  shows its own value (from its `.grid` sidecar; paintball panels have no
  sidecar yet so show nothing). Verified through the real viewer. **Still
  open**: a dedicated toolbar icon (reuses `grib.png`), and a paintball
  sidecar (members-exceeding count).
- **Stage 6 (original text)** — the 4-panel mosaic export (single JXL / animated JXL of the
  whole comparison, per "Export formats" above), `AnimationBar` range-export
  applied per-panel too, toolbar entry (new icon; button placed near "RRFS
  GRIB Viewer" in `src/ui/Toolbar.cpp`).

### Open questions to settle at Stage 0

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

### Status check, 2026-10-01 (supersedes the "still open" notes above where they differ)

Verified by rendering every row live (37 rows; one member stands for the five): all render.
- **Done, contrary to the older notes**: the dedicated Member 1-5 picker (RefsPanel's third combo), the toolbar icon
  (`refs.png`), `lpmm`/`avrg` precipitation rows, REFS probability rows for precipitation/snow/freezing rain.
- **Done 2026-10-01**: the GRIB viewer's smoothing is shared (cubic resampling at 2 px per native cell, hover grid
  width from `UtilityGrib::sampleGridColumns`); a **Compare** dropdown (change vs the run 6/12/24 h earlier) for
  the plain panels via `UtilityRefs::renderDifference`; a Sounding button; `finishRender` now rides on the shared
  `UtilityGrib::colorizeToPng`, and render() was split into `selectField` + `plainRaster`.
- **Still open**: `eas`/`ffri` (blocked on what the published thresholds mean); change maps for paintball and
  member-probability panels (derived per member - they say so on screen); a user-chosen comparison run.

## 17. Sounding viewer: remaining SPC panels (sketch, 2026-10-01)

_(Was `docs/%s`, folded in here on 2026-11-14.)_

Done: skew-T on SPC's axes, hodograph, 1 km / 6 km wind inset, grid tables, annotations (precip type, layer labels),
wind speed vs height bars (`drawWindSpeed`, panel rect 595,25,93,565).

All remaining panels are ports from SHARPpy (BSD, see `sharppy-notice.md`). Never guess a formula; port the source.

### Next: inferred temperature advection column (rect 688,25,67,565)
Source: `sharptab/params.py: inferred_temp_adv(prof, dp=-100, lat=35)`, panel `viz/advection.py`.
- Layers every 100 mb from the surface up to 100 mb: for each pair (p_bottom, p_top)
  - mean wind of the layer (mass-weighted `winds.mean_wind`), geostrophic, speed in m/s
  - `d_theta` = change of wind direction with height: rotate so the bottom direction is 180, then
    `d_theta = top_dir - 180` (wrapped to 0..360 first)
  - `multiplier = (f / 9.81) * (pi / 180)`, `f = 2 * omega * sin(lat)`, `omega = 2 pi / 86164`
  - `t_adv = multiplier * speed^2 * avg_temp * d_theta / (z_top - z_bottom)` K/s, times 3600 for C/hr
    (SHARPpy's `avg_temp = (T_bottom + T_top) * 2` in K is kept as written, not "fixed")
- Panel: x scale -13..+13 C/hr (SHARPpy `adv_min/adv_max`), dashed centre line at 0, one bar per layer on the skew-T's
  pressure scale; warm advection red, cold advection `#3399CC`, value printed beside each bar; title "Inf. Temp. Adv. (C/hr)".
- Needs: station latitude. Observed soundings: SoundingSites has lat/lon; model soundings: the picked point's `lat`.
  `SoundingProfile` gets a `latitude` field (default 35 as SHARPpy does). Note: SHARPpy's values are much smaller than SPC's
  (documented in its source); label it as inferred.
- New code: `src/sounding/SoundingAdvection.{h,cpp}` (pure function + layer struct, unit-checkable against a hand case),
  `drawTempAdvection()` in `SoundingViewer.cpp` next to `drawWindSpeed()`.

### After that
1. theta-e vs pressure with TEI (`viz/thetae.py`, `params.py: thetae_diff`/TEI) in the strip under the wind panels.
2. storm-relative winds vs height with the classic-supercell envelope (`viz/srwinds.py`).
3. SHIP and effective-STP box plots (`viz/ship.py`, `viz/stpef.py`; the percentile data is in those files).
4. SARS analogs (`viz/analogues.py`, `databases/sars_supercell.txt`, `sars_hail.txt`, matching in `sars.py`).
5. SPC-style bottom tables (MU parcel row, K index, MidRH/LowRH, SigSevere, ESP, MMP, WNDG, NCAPE, Corfidi vectors, MPL).
Each panel is one commit: port, draw, headless screenshot (`WXQT_OPEN=spcsoundings.png WXQT_GRAB=...`), compare with SPC's graphic.

## 18. Tropical Hub - idea and sketch (2026-10-02)

_(Was `docs/%s`, folded in here on 2026-11-14.)_

Status: **idea, nothing started.** Written down so it can be read again and decided on. Part of the ideas list in
`outstanding-work.md` (status board).

### The idea
Generalize today's **Tropical** screen into a **Tropical Hub**: one place for current storms, historical tracks, seasons, model
guidance, recon and the ocean conditions around storms. The user knows and likes the layout of CyclonicWx (cyclonicwx.com), and
wants an own source of the same kind of information, separate from that site.

**Ground rule:** the structure (sections, kinds of graphics, how a user moves around) follows what that site does; everything else
is our own - our layout, wording, colours, charts and code, built from public NOAA / agency data. No copying of their images,
text or code, and nothing here claims their site is built from the same data (it does not say where its data comes from).

### Two main screens, one app (decision to confirm)
- **Tropical Hub** (storms): active storms, storm pages, historical tracks, the season board, guidance, recon.
- **Climate and ocean** (exists, `src/climate/`): ENSO status, index charts, SST and anomaly, marine heat, currents, MJO, SST
  history loops.
- **Why two screens:** different pace (storms change hourly, climate daily to monthly), different weight (the climate screen loads
  ~25 MB of pictures on open; the hub should open instantly), and a hub with many tabs plus the ocean tabs would be unwieldy.
- **Why one app:** they share the map code, the zoom / Save viewer, the animation renderer, the chart widget, download throttling
  and settings. Packages are 47 MB (Windows) / 66 MB (Linux). Splitting later would be mostly packaging (the code is already in
  `src/tropical/` and `src/climate/`); revisit only if one needs its own release schedule or is to be handed out alone.
- **Making them a pair:** no duplicate SST screens - the hub gets a tile "SST and heat around this storm" that opens the climate
  screen, and the climate screen gets a tile for the ENSO state's effect on the season. A button on each opens the other (the
  Tropical screen has one today). One toolbar group "Tropical and ocean" with two icons.

### The hub: tabs (modelled on the structure of the site the user knows)
| Tab | What it shows | Our source | Status today |
|---|---|---|---|
| **Storms** (home) | Cards for each active storm (position, wind, pressure); season table by basin (dates, peak wind, pressure, ACE); ACE-by-year chart | CIRA / RAMMB list (have); NHC best-track files; IBTrACS for past years | Cards exist; season table and ACE chart new |
| **Storm page** | Satellite, track so far, pressure and wind history charts, model-track guidance (spaghetti), official cone, wind radii, parameters table (position, pressure, wind, radius of max wind, environmental pressure, storm size) | CIRA, JTWC and JMA pages (have); NHC best-track and guidance files | Partly have; track history, intensity charts, parameters, spaghetti new |
| **Satellite and radar** | Loops | CIRA loops, GOES, our radar | Have |
| **Models** | Tropical model fields: winds, shear, vorticity, moisture, forecast-hour slider, multi-model compare | GFS / GEFS from NOMADS; other models have separate open feeds (unchecked) | GRIB viewer is RRFS-only and multi-model is **on hold** - needs a decision |
| **SST and ocean** | SST, anomaly, ocean heat content (300 m), 20 C isotherm depth, ENSO longitude index, region and period menus, regional-mean time series | The Climate screen (SST, anomaly, history loops, Nino charts) | Mostly have; heat content, isotherm depth, ELI, regional time series new |
| **Recon** | Aircraft reconnaissance observations when present | NHC reconnaissance products | New, **unchecked** |
| **Observations** | Station reports near a storm | Our observation data | Partly have |

### Use case: current and historical tracks
- **Now:** where the active storms are, best track so far, forecast cone, and the spread of model tracks, coloured by intensity.
- **Then:** pick a basin and year, or a storm name, and see the tracks; filter by category; "storms that passed within N km of
  my location".
- **Season:** storms / hurricanes / major hurricanes / ACE by basin, and ACE across years (reuses the bar-chart widget built for
  the ENSO indices).
- Everything saves with the dated names and can use the animation renderer (e.g. a season played storm by storm).

### Data (verified 2026-10-02 unless noted)
- **IBTrACS** (NOAA NCEI) CSV by basin, v04r01: North Atlantic 57 MB, East Pacific 47 MB, West Pacific 114 MB, whole world 333 MB,
  "last 3 years" 11 MB. The `ACTIVE` file (316 KB, current storms only) and `last3years` were both modified 2026-10-02 01:47 UTC, so they refresh at least daily.
- **NHC ATCF** (`ftp.nhc.noaa.gov/atcf/`): `btk/` the live best track of each current storm and invest (e.g. `bep132026.dat`),
  `aid_public/` model guidance, `archive/`.
- **HURDAT2** (NHC) for the official Atlantic / East Pacific record.
- **Not reachable from here:** the NOAA SSD mirror of the JTWC best-track files, so the western Pacific live track relies on the
  JTWC and JMA pages already integrated (and ATCF-format files from another route, if one is found).
- IBTrACS is large: download once per basin, convert to a compact local cache, refresh occasionally.

### Recon: findings (checked 2026-10-02)
- **Live feed:** `tgftp.nws.noaa.gov/data/raw/ur/` keeps only the **latest message** of each type, overwritten as new ones arrive:
  HDOB (high-density observations: `urnt15` Atlantic, `urpn15` East Pacific, `urpa15` Central Pacific; 12 lines of 30-second
  observations each), vortex data messages (`urnt12`), mission position / observation lines (`urnt10` / `urnt11`), supplementary
  vortex messages (`urnt14`). So a live mission view has to **collect messages as they arrive** (the Tropical Tidbits recon page
  says it updates every 10 minutes while a mission is flying, with the raw data courtesy of NHC).
- **Archive:** `nhc.noaa.gov/archive/recon/<year>/<type>/`, years **1989-2026**, one file per message named
  `<type>-<KWBC|KNHC>.<yyyyMMddHHmm>.txt`, directory listings work. Types seen: `AHONT1` (HDOB Atlantic; 3,748 files so far in
  2026), `AHOPN1` (East Pacific HDOB; 7,358), `AHOPA1` (Central Pacific), `REPNT2` (vortex data messages; 47), `REPNT3` (dropsonde
  reports in the WMO "TEMP DROP" code; 369), `REPNT0` / `REPNT1` (mission position and observation lines). The archive is also
  the way to fill in the minutes before the viewer was opened. Only 2026 contents were looked at; older years are unchecked.
- **HDOB lines** carry time, position, static pressure, geopotential height, temperature, dew point, flight-level wind, peak wind,
  and the SFMR surface-wind and rain-rate fields, plus quality flags. The field meanings must be **ported from NHC's HDOB
  format description, not guessed** (standing rule).
- **What a Recon tab could show** (the kinds of graphics the user knows from Tropical Tidbits; its recon page was empty when
  checked, so the exact graphics there are unconfirmed): the mission list (aircraft, storm, times); a map of the flight track
  coloured by flight-level wind or SFMR wind with the storm centre; time series of pressure, wind, temperature / dew point and
  SFMR; a vortex-message table (centre fix, minimum pressure, maximum winds); dropsonde profiles drawn with our existing
  skew-T engine (needs a decoder for the TEMP DROP code).
- **Size of the work (rough):** HDOB parser and mission collector small-to-medium; flight-track map and time-series charts medium
  (the chart widget and map code exist); vortex-message parser small; dropsonde decoder + skew-T medium-to-large.
- **Risks:** the live files are overwritten (collecting must run while the app is open, or fall back to the archive's listing);
  an archive year is thousands of small files (list, then fetch only the missions asked for, paced); mission-to-storm matching
  needs the storm id from the vortex message / mission header.

### Phases
1. **Storms tab:** cards, season table by basin, ACE chart (IBTrACS + NHC files).
2. **Storm page:** track history, wind / pressure charts, model-guidance spaghetti, parameters table.
3. **Historical map:** basin / year / name filters, category colouring, "near me".
4. **SST and ocean additions** (heat content, isotherm depth, regional time series), then the other tabs.
5. Extras: intensity trace, season loops, Models and Recon tabs.

### Risks and open questions
- **Definitions, not guesses:** ACE and the intensity categories come from the agency definitions. IBTrACS carries winds from
  several agencies that do not always agree - a rule for which one to show is needed.
- **ENSO longitude index (ELI)** is a published index: its formula must come from the paper, not be guessed.
- **Ocean heat content / 20 C depth:** source not confirmed (AOML's tropical-cyclone-heat-potential pages returned an
  authentication error).
- **Western Pacific live tracks:** gap described above.
- **Models tab:** needs the multi-model decision (feature-scope B1, on hold) or a narrow tropical-only version.
- **Recon:** see the findings above (data is public and complete; the decoding must be ported from NHC's format descriptions).
- **Which first?** Suggested: Storms tab (season table + ACE) and the current-tracks map.

### Related code and notes
- Existing: `src/tropical/` (TropicalViewer, CiraStorm, CiraLoopViewer, UtilityCira, UtilityJma), `src/nhc/` (NHC tool),
  `src/climate/` (ClimateViewer, ClimateChart, ClimateHistoryViewer, UtilityClimate).
- Other open-source projects looked at (references only, none integrated): wptc-track (GPLv2+, NHC track maps), CyTRACK (GPLv3,
  Python), GFDL TCtracker (GPLv2 only - not compatible with this GPLv3 app), besttracks (Python); Supercell Wx (MIT, radar).

---

### Expansion (2026-10-02): directories, layouts, recipes

All links below were requested on 2026-10-02 and answered 200 unless marked. "Listing" = the directory index works in a browser
and can be read by the app. None of this is built; it is the reference sheet for building it.

### A. Data directories and files (verified)

#### NHC ATCF FTP - the live storm database (`https://ftp.nhc.noaa.gov/atcf/`)
| Directory / file | What is in it | Note |
|---|---|---|
| [`btk/`](https://ftp.nhc.noaa.gov/atcf/btk/) | Best track so far, one file per storm or invest: `bep182026.dat` = East Pacific storm 18 of 2026; `bal912026.dat` = Atlantic invest 91. Subdir `cphc/` for the Central Pacific. | One row per time and wind-radius threshold (three rows per time in the sample, for 34 / 50 / 64 kt). Updates every advisory / special fix. |
| [`fst/`](https://ftp.nhc.noaa.gov/atcf/fst/) | Official NHC forecast, `ep182026.fst` (rows tagged `OFCL`, forecast hours 0, 12, 24 ...) | Gives the forecast track and wind radii for the line and cone. |
| [`aid_public/`](https://ftp.nhc.noaa.gov/atcf/aid_public/) | Model guidance tracks, gzipped, `aep182026.dat.gz` | One file holds every model; the 5th column is the model name (counted in a live file: GDMN, AVNO, NVGM, CTCX, plus 30-odd ensemble members `AP01`...`AP30`). This is the "spaghetti". |
| [`archive/`](https://ftp.nhc.noaa.gov/atcf/archive/) | One folder per year (`archive/2025/`) with the final best track and guidance per storm, plus `HURDAT/`, `MESSAGES/`, `store_invest/`, `storm.table`, `README` | `archive/<year>/messages/` has the text advisories (`al012025.discus.001` = discussion 1); `archive/<year>/tcvitals/` the vitals files. |
| [`docs/`](https://ftp.nhc.noaa.gov/atcf/docs/) | `README-dataFormats` (only points to NRL), **`nhc_techlist.dat`** (every model code with a long name: `OFCL` = NHC official forecast ...), `tcdevel.dat`, tracker-change PDFs | The tech list is how model codes become readable labels - use the file, do not guess names. |
| [`index/`](https://ftp.nhc.noaa.gov/atcf/index/) | Storm lists, `AL_storms.txt.nhc` etc. | Lists the storms of the season by basin. |
| [`wndprb/`](https://ftp.nhc.noaa.gov/atcf/wndprb/) | Wind-speed probability text, `al012026.wndprb.001` | |
| [`pub/`](https://ftp.nhc.noaa.gov/atcf/pub/), [`adv/`](https://ftp.nhc.noaa.gov/atcf/adv/), [`stext/`](https://ftp.nhc.noaa.gov/atcf/stext/), [`lsdiag/`](https://ftp.nhc.noaa.gov/atcf/lsdiag/), [`gpce/`](https://ftp.nhc.noaa.gov/atcf/gpce/) | Public advisories (`pub/al012026.public.001`), advisory info XML (`adv/*_info.xml`), SHIPS intensity text (`stext/..._ships.txt`), SHIPS diagnostics (`lsdiag/`), forecast-error radii (`gpce/`) | SHIPS = the statistical-dynamical intensity guidance; its text carries shear, SST, humidity predictors - a ready "environment" panel. |
| [`com/`](https://ftp.nhc.noaa.gov/atcf/com/) | Working tcvitals / command files | Mostly internal; low priority. |

**Format authority:** the column layout is defined by the Naval Research Laboratory's ATCF database document. The NHC copy is
`atcf/docs/NRL_doc_ATCFdatabase.html` (listed in the docs directory), and NHC's own readme points to
`nrlmry.navy.mil/atcf_web/docs/database/new/database.html` (that address answered 302 from here; it redirects, so follow it).
The parser must be written from that document. A live row looks like: basin, storm number, time as yyyymmddhh, technique,
forecast hour, latitude and longitude with N/S/E/W letters in tenths of a degree, wind (kt), pressure (mb), a status code
(`HU`, `TS`, `TD` ...), then the wind-radii quadrant fields; the **exact meaning of every later field comes from the document**.

#### NHC web data (`https://www.nhc.noaa.gov/`)
| URL | What | Note |
|---|---|---|
| [`CurrentStorms.json`](https://www.nhc.noaa.gov/CurrentStorms.json) | The active-storm list, one entry per storm: id (`ep182026`), name, classification, intensity, pressure, lat/lon, movement, `lastUpdate`, and links to the advisory, forecast advisory, wind probabilities, discussion, graphics | The best single "what is active now" feed - tidier than scraping. Entries are per advisory. |
| [`gis/`](https://www.nhc.noaa.gov/gis/) | The GIS index page. Direct files: `gis/best_track/<id>_best_track.zip` / `.kmz`, `gis/forecast/archive/<id>_5day_latest.zip` (cone + track + watches/warnings as shapefile), `<id>_fcst_latest.zip`, `latest_wsp34knt120hr_5km.kmz` (wind-speed probabilities), `gis/kml/nhc_active.kml`, `gis-at.xml` / `gis-ep.xml` / `gis-cp.xml` (RSS of GIS products by basin) | Shapefiles need a reader (GDAL is already a candidate dependency - see the SFCOA note in the status board); the KMZ files are zipped XML and cheaper to read. |
| [`gis/best_track/`](https://www.nhc.noaa.gov/gis/best_track/) (listing, 0.5 MB), [`gis/forecast/archive/`](https://www.nhc.noaa.gov/gis/forecast/archive/) (listing, 19 MB - **do not list it often**) | Per-storm files | Past advisory cones are kept per advisory number. |
| [`gis/archive_besttrack.php`](https://www.nhc.noaa.gov/gis/archive_besttrack.php), `archive_forecast.php`, `archive_gtwo.php`, `archive_wsp.php`, `archive_psurge.php` | Archive pages by year | Human pages; use for finding the file naming, then fetch directly. |
| [`gtwo.xml`](https://www.nhc.noaa.gov/gtwo.xml) | Tropical Weather Outlook feed with the areas of possible development | The "what might form" layer. |
| [`data/hurdat/`](https://www.nhc.noaa.gov/data/hurdat/) | The official HURDAT2 files, several dated versions of each; format PDFs `hurdat2-format-atlantic.pdf` and `hurdat2-format-nencpac.pdf` (listed there) | **Pick the newest by date in the name** (Atlantic `hurdat2-1851-2025-092326.txt`, East/Central Pacific `hurdat2-nepac-1949-2025-092926.txt` at the time of checking). The listing grows with every revision. |
| [`data/tcr/`](https://www.nhc.noaa.gov/data/tcr/) | Tropical cyclone reports, one PDF per storm and year | Link-out only (storm page "official report"). |
| [`climo/`](https://www.nhc.noaa.gov/climo/), [`climo/images/`](https://www.nhc.noaa.gov/climo/images/) | Climatology graphics (e.g. average formation maps) | Pictures to link in the season screen; the file names were not catalogued. |
| [`tafb_latest/`](https://www.nhc.noaa.gov/tafb_latest/) | Tropical Analysis and Forecast Branch charts | Already used for the SST loop. |

#### Recon (`checked in the earlier recon pass; re-verified now`)
- Live (overwritten): [`tgftp.nws.noaa.gov/data/raw/ur/`](https://tgftp.nws.noaa.gov/data/raw/ur/) - files like `urnt15.knhc..txt`
  (HDOB, Atlantic), `urnt12.knhc..txt` (vortex message), `urnt10/11` (RECCO), `urnt14`; `urpn*` East Pacific and `urpa*` Central
  Pacific and Guam (`pgua`) variants.
- Archive: [`nhc.noaa.gov/archive/recon/`](https://www.nhc.noaa.gov/archive/recon/) with folders **2020-2026** at this check
  (the earlier note said 1989; the root listing now shows 2020 onward - older years need a separate look), and the
  [`readme.txt`](https://www.nhc.noaa.gov/archive/recon/readme.txt) lists every report code with its WMO header:
  `AHONT1`/`AHOPN1` = HDOB (URNT15 / URPN15), `REPNT2` = vortex data message (URNT12), `REPNT3` = dropsondes (URNT13),
  `REPNT0`/`REPNT1` = RECCO, `REPRPD` = **Plan of the Day** (the scheduled missions - the mission list for free). The readme
  notes the archive is created in real time and **not organized by storm name**.
- Format authority: the National Hurricane Operations Plan (NHOP) describes every product; NHC's readme points to it.
  The HDOB field order and flags are to be ported from there, not inferred.

#### Historical record
- IBTrACS v04r01: [`.../v04r01/access/csv/`](https://www.ncei.noaa.gov/data/international-best-track-archive-for-climate-stewardship-ibtracs/v04r01/access/csv/)
  holds `ibtracs.ACTIVE.list` (316 KB), `last3years` (11 MB), `since1980`, `ALL`, and per basin `NA`, `EP`, `WP`, `NI`, `SI`,
  `SP`, `SA`, plus `IBTrACS_SerialNumber_NameMapping_v04r01_<date>.txt` (the name-to-serial lookup, dated 20261002, so it is
  rebuilt daily). Documentation: [`.../v04r01/doc/`](https://www.ncei.noaa.gov/data/international-best-track-archive-for-climate-stewardship-ibtracs/v04r01/doc/)
  with `IBTrACS_v04r01_column_documentation.pdf` (what each column means and which agency's value it is),
  `IBTrACS_version4r01_Technical_Details.pdf`, and the change log. **Plan:** `since1980` for the "last decades" screens and one
  basin file only on demand; the 333 MB `ALL` file is not needed.
- HURDAT2 (above) for the official Atlantic and East/Central Pacific record; ATCF `archive/` for per-storm best tracks.

#### Satellite and storm pages (existing code, listed for completeness)
- CIRA / RAMMB real-time: [`rammb-data.cira.colostate.edu/tc_realtime/`](https://rammb-data.cira.colostate.edu/tc_realtime/)
  (the storm list that `UtilityCira` already reads; a sub-path `products/storms/` answered **403** - only the pages the code
  already uses are open). Also see RAMMB-SLIDER, linked from that page.
- JTWC (`metoc.navy.mil`) answered **403** from here and the SSD mirror did not answer (no connection) - the western Pacific live
  track still depends on the JMA/JTWC pages already integrated.
- OSPO Coral Reef Watch folder listing is blocked (403) but the file addresses used by the climate screen work.
- AOML pages: `aoml.noaa.gov/phod/cyclone/data/` answered **401** (login), as before; `aoml.noaa.gov/phod/hdob/` is a 404.
  So heat potential comes from somewhere else (open question stays open).
- Model fields for tropical maps: NOMADS directories `nccf/com/gfs/prod/` and `nccf/com/hafs/prod/` answered 200 with an empty
  body to a plain request - they are served but need the right sub-paths (dated folders); not explored yet.

#### Reference pages (for how the user's mental model works - look, do not copy)
- Tropical Tidbits: [`/storminfo/`](https://www.tropicaltidbits.com/storminfo/) (storm pages) and [`/recon/`](https://www.tropicaltidbits.com/recon/)
  (recon). Their pages are JavaScript-built, so a plain fetch shows no content; open them in a browser to see the layout.
- CyclonicWx (cyclonicwx.com): the layout the user knows; not fetched. Tabs for storms, guidance, SST/ocean, models.

### B. Screen sketches (own layouts; structure follows the user's reference)

**Storms (home).** Top strip: one card per entry in `CurrentStorms.json` (name, class, wind, pressure, position, movement,
advisory time, a small satellite thumbnail). Below: a map of the basin with the best-track line (coloured by category), the
official forecast line and points, the cone, and the guidance models as thin lines. Right column: season table (counts by basin
for the year) and the ACE bar chart by year. A basin selector (Atlantic / East Pacific / Central Pacific / West Pacific) and a
"My location" marker.

**Storm page** (click a card). Header: name, id, advisory, links to the discussion / public advisory (from the JSON). Tabs:
*Track* (map + the guidance and the cone, with a "model" check-list so lines can be turned on and off), *Intensity* (wind and
pressure vs time, best track so far with the official forecast and the SHIPS forecast on the same axes), *Structure* (wind-radii
table by quadrant from the best-track rows), *Environment* (the SHIPS text: shear, SST, humidity), *Satellite* (the existing
CIRA loop), *Recon* (when flights exist).

**Historical.** Basin + year + name pickers, a category filter, a "within N km of a point" box. Map of all matching tracks;
click a track for its storm page (from IBTrACS rather than ATCF). Table beside it (name, dates, peak wind, minimum pressure, ACE).

**Seasons.** Per basin: bars of storms / hurricanes / major hurricanes per year, ACE per year, and a "this year so far vs the
average" line. This is the screen the ENSO status can link to.

**Recon.** Mission list from `REPRPD` (plan of the day) and the files in the archive; selecting a mission loads its HDOB lines
onto a flight-track map coloured by flight-level or SFMR wind, with time-series strips and the vortex-message table.

**SST and ocean.** The existing Climate screen, linked from a storm page ("SST around this storm").

### C. Building blocks that exist, and what each needs

| Need | Exists | New work |
|---|---|---|
| Storm list | `UtilityCira` (reads CIRA); `src/nhc/` | A `CurrentStorms.json` reader (the app has JSON helpers for IEM warnings) |
| Track map | Map projection and layer code in the radar / `src/map/` areas | A "tracks" layer drawing polylines with category colours |
| Charts | `ClimateChart` (bar chart widget) | A line-chart variant for wind / pressure vs time |
| Pictures with zoom / save / loop | `ZoomOnDoubleClick`, `AnimationRenderer`, `ClimateHistoryViewer` | None |
| Downloads | `FutureBytes` / `FutureText` / `FutureVoid` | A small on-disk cache for the big files (IBTrACS, HURDAT2) |
| ATCF / HURDAT2 / IBTrACS parsers | None | New, from the format documents (the main work of phase 1) |
| Shapefile cone | None | GDAL or the zipped KMZ route (decide with the SFCOA reader) |

### D. Phase 1 in detail (suggested first build)

1. `UtilityTropical`: read `CurrentStorms.json` -> `ActiveStorm` list. (small)
2. ATCF reader for `btk/` and `fst/` per the NRL document: best track, official forecast, wind radii. (small-medium)
3. Storm cards + a track map for the selected storm. (medium)
4. `aid_public/` reader and a model check-list, with names from `nhc_techlist.dat`. (small-medium)
5. Season table and ACE from HURDAT2 / the current `btk/` files, using the agency's ACE definition. (medium)
6. Settings: basin choice, cache folder, refresh interval; all requests paced and with the user-agent rule.

**Tests that work headless:** all parsers can be unit-checked against files pulled today (the active Pacific storm `ep182026`
is a live example); screenshots with the existing offscreen method.

### E. Open questions to settle before building
1. Open the hub as a separate toolbar entry from Climate (recommended above), or as tabs inside one window?
2. Which agency's wind for historical tracks (IBTrACS column documentation lists the choices).
3. Cone: read the shapefile (GDAL) or the zipped KMZ.
4. Western Pacific live data source (JTWC blocked; JMA only?).
5. Do we keep the recon tab for later (suggested) and ship Storms + Storm page + Historical first?

### F. Recon "pending plans" = the Tropical Cyclone Plan of the Day (TCPOD) (checked 2026-10-02)
This is the product for planned / requested flights. Header `NOUS42 KNHC`, code **`REPRPD`**, issued by CARCAH (NHC) about once a
day (more when changed), text only.
- **Live address:** `https://www.nhc.noaa.gov/archive/recon/<year>/REPRPD/REPRPD.<yyyyMMddHHmm>.txt` (listing works; 251 files so
  far in 2026, newest `REPRPD.202610021345.txt`). Not found among the `tgftp.../raw/ur/` files.
- **Layout:** header with issue time, `VALID dd/hhmmZ TO dd/hhmmZ`, `TCPOD NUMBER 26-124`; then `I. ATLANTIC REQUIREMENTS` and
  `II. PACIFIC REQUIREMENTS`, each with numbered items per storm, `OUTLOOK FOR SUCCEEDING DAY`, and `REMARKS` (cancellations,
  research missions).
- **Quiet day (today):** both basins read `NEGATIVE RECONNAISSANCE REQUIREMENTS`. The Recon tab must show "no flights planned".
- **Busy example (2026-09-27 14:47Z):** `1. HURRICANE POLO` / `FLIGHT ONE - NOAA 42` with lines `A.` to `H.`:
  `A. 28/1800Z`, `B. NOAA2 0517E POLO`, `C. 28/1330Z`, `D. 25.5N 112.9W`, `E. 28/1600Z TO 28/2000Z`,
  `F. SFC TO 10,000 FT`, `G. TAIL DOPPLER RADAR & FIX`, `H. NO WRA ACTIVATION`; remarks told of a canceled mission.
- **Letter meanings (A-H) must be taken from the NHOP** (National Hurricane Operations Plan), not inferred from this one example.
- **Tab use:** a "Pending missions" panel parsed from the newest file, mission rows (aircraft, storm, times, target point, altitude,
  purpose), a marker on the map at the target point, and remarks shown as text; the parsing is small (line-based). History of
  plans from the archive listing.

## 19. The polar vortex: the initial state of the global models since a date (idea, 2026-11-14; nothing written)

**The wish:** watch the stratospheric polar vortex through the season: the f000 (the initial state) of the global models, as a history from a chosen date to now.

**What is already there.** (1) Initial states: `GFS-ANL` (the GFS f000), `GFS1-ANL`, `CFS-ANL`, and ERA5 at NCAR (hourly, 37 levels, back to 1940, read through the `pl:` files: any "N mb" level of HGT, TMP, UGRD, VGRD works). (2) A chart for a valid time ("by the clock", `Variant::validTime`): the screen already asks for a model's run by the time it is valid. (3) A `POLAR` sector (55 to 82 N, as a Mercator rectangle) and, on the master map, the polar projections. **Checked on the NOAA archive (2026-11-14):** the GFS f000 file carries HGT, TMP, UGRD and VGRD at 1, 2, 3, 5, 7, 10, 20, 30, 50, 70 and 100 mb, and the archive on S3 reaches back at least to April 2021 (`gfs.20210401/00` answers). The ECMWF open data has 50 mb as its highest level.

**What is missing.** (1) No chart above 200 mb in the catalogue (nothing at 100, 70, 50, 30, 10 mb). (2) No loop over dates: the loop is over forecast hours of one run; a history is one frame per valid time (every 6 hours), each its own f000. (3) No numbers of the vortex: the winds at 10 mb and 60 N (the zonal mean: a sudden warming is when it turns to easterly), the temperature of the polar cap (60 to 90 N), the height at 10 mb of the pole.

**In steps, each useful alone:** (1) the stratosphere charts: 100, 50, 30, 10 mb height, wind and temperature (the temperature filled, the heights as lines, the wind as barbs or streamlines), for the GFS and so for the models cloned from it, ERA5 and the analyses; on the polar projection of the master map, since a Mercator rectangle cuts a vortex across the pole into stripes. (2) A history player: a model that has an initial state, a start date and a step (6, 12, 24 hours): frames by valid time, looped and saved as an animation; `validTime` is the way in, the fetch is a box of the grid (a few levels' bytes), so a season is hundreds of small reads, not downloads of whole files. (3) A strip of numbers under the loop: the zonal mean wind at 60 N and 10 mb and the polar cap temperature, one point for each frame, with the line and its dates. (4) Perhaps: the vortex edge (the 10 mb heights' lines, or the potential vorticity), and the forecast on the same strip (the f000 history, then the model's forecast continuing it).

**Open questions:** the start date to default to (1 October?) and whether the history is of the GFS only or compared with ERA5 for the same dates (the reanalysis is the better truth, a few days behind); the levels (10 and 50 mb first, then the rest); whether the history player is its own screen or a mode of the Model Viewer's loop.

