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

Done and in the working tree, **not committed**:

- `gfs/ChartSelection` (new): the model, the chart, the ticked lines, the other models' lines, the member, standard deviations, the change since an older run, the day's maximum, the area
  and the valid time, as one object with a line form (a saved view's) and the one function that makes the renderer's request (`variant`).
- The map's model layer holds its state through it (`selection()`, `apply()`), its saved-view line and its request to the renderer come from it. Saved views keep their format.
- The handoffs carry it: "On the map..." gives the layer the whole selection; the map's "Other models... > Model Viewer" gives the viewer the lines, other models, standard deviations, the
  change and the member (`ModelViewer::applyVariants`, the member once the model's list is read).
- The master map says what time each layer is drawing against the bar's (a line under each layer that is on, amber when it is behind), what kind of time the bar is on, where "now" is on the
  strip, and "Latest" is the frame of now when forecasts and observed layers are on together.
- Started, compiles, not on screen yet: `ModelChartLayer::controls(parent, changed, asBar)`: the layer's choices as its panel (as before) or as a bar (the same widgets and handlers side by
  side on a flow layout), `ModelChartLayer::bar()`, and `refreshers` so the panel and the bar show each other's changes.

Not checked in the running program: the two handoffs clicked through; loops and the From-to range with the new "Latest"; the bar anywhere.

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
3. **The Model Viewer on the same selection.** (first part done 2026-11-10: the four fields are one `ChartSelection picked` and `selection()` starts from it; a change of other than 6, 12 or 24 hours gets its own item in the box, and the day's maximum is taken (waits for the hours if they are not known yet; both built, not clicked); the renderer's request of a plain or change chart comes from `selection().variant` (a maximum keeps the viewer's own hours); left: the model, chart and area from `ChartSelection::variant`.)
   - Its own members (`overlays`, `overlayModels`, `member`, `standardDeviations`, `variant`, the model, chart and area of `objectModel`) become one `ChartSelection`, and its requests to the
     renderer come from `ChartSelection::variant` (the tiles of a compare add their own on top). The differences that stay are named in the object: the difference from another model or
     run, the same valid time across models, the hidden layers of a paintball, the pooled ensembles' choice, the fitted legend.
   - The model, chart and area of a chart handed over are set from the selection, not by writing `NCEP`, `MODELNCEPPARAMLASTUSED` and `MODELNCEPSECTORLASTUSED` first (the Tropical Hub
     and the history page do the same and move to the selection too).
   - The day's maximum comes back from the map; a change of other than 6, 12 or 24 hours is kept, not dropped.
   - Check: `tests/gfs` still pass; each menu of the viewer on GFS, REFS, a storm model, a local area and a picture model; the settings kept between runs.
4. **The bar is one piece.** The Model Viewer's top rows and the map's bar become the same widget (`models/ModelBar`), bound to a `ChartSelection`, the run and the hour. The layer's
   `controls(..., asBar)` is the first form of it; the viewer's rows (the model box, the run box, the chart button, the area button, the time strip, member, change) are moved onto it one at a
   time, each a commit that changes nothing on screen.
5. **Picture / Map in the Model Viewer.** A switch under the bar: the picture is replaced in place by the map widget with the same selection as its model layer, and "Add layers..." offers
   the rest of the map's layers. "On the map..." becomes that switch.
6. **One window.** The model screen and the master map are the same screen opened two ways (step 5's switch and the layer list shown or hidden). The source box of the model screen (SPC
   Post, NSSL CAMs, the picture models) stays as pages beside it.

### Open points

- **Compare tiles.** Four pictures side by side have no map form beyond the map's two maps. Either the tiles stay a thing of the picture canvas, or the map gets panes.
- **The legend.** The picture's fitted legend (the range of what is on view) needs a place on the map canvas; the map's legend strip shows the whole scale.
- **Exports.** "Export view" of the map already saves the Model Viewer's titled picture (`titled()`); with one screen there is one Save.
- **Storm models.** Their area is the storm; on the map the view follows the storm's grid. Not designed.
- **Size.** `ModelViewer.cpp` is 3,800 lines whose rows are tied to its combo boxes and `objectModel`; step 4 is the long one. Steps 1 and 2 give the familiar controls on the map without it.

### Also from this look (not part of the steps above)

- The model family: carry the valid time and the area between the pages of the model screen; fold SPC Post into the Model Viewer (it is there already as the models `SPCPOST` and
  `SPCGEFS`); list the 15 minute window and the analysis loop as pages; open MRMS verification on the chart, area and time on view.
- The hurricane screen and the models: "HAFS for this storm" and "Model charts here" on the hurricane screen, "Storm page..." in the Model Viewer on a storm model, HAFS-A and B as a
  family of the hurricane intensity chart (one intensity chart, not two).
- The master map: the layers' shared choices (opacity, order) in the manager, a search box and the layers last used, "Show on the map" from the radar, outlook and satellite screens.
