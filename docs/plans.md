# Plans

What was planned and what is left. Check the code before acting on a line.

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
