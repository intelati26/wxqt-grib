# Plans

What was planned and what is left. Check the code before acting on a line.

**Contents** (status as of 2026-11-19; the sections after 24 were added to this list then)

- 1. Fold the NSSL tar loader into the main GRIB path (done)
- 2. NetCDF sources: what could be combined (SFCOA / RRFSD done)
- 3. One GDAL raster reader for the NetCDF (and tar) flows (done, but the GOES layer)
- 4. Model verification against MRMS (reflectivity)
- 5. Open-Meteo's `.om` files on S3: grids with no API calls (done 2026-11-05, `d864da8`)
- 6. One model screen over two canvases (step 6 closed 2026-11-10: the picture stays its own screen; small items left)
- 7. WOOF-NAM from its own site (done 2026-11-11 as `WOOF-NAM-HOSTED`; status.json and the hindcasts left)
- 8. Extended point view from the NBM text bulletins (built 2026-11-12, polished 11-15; on 11-16 the RRFS smoke+dust line, the precipitation type colours and the map of the picked time beside the table; left: the NBH, asked and not answered)
- 9. Charts as presets of the checks (the Checks picker and the alias table built 2026-11-13 to 15)
- 10. One model's field over another model's chart (**complete** 2026-11-16: the picture, the map layer, the whole-world export and its history pictures, the compare tiles; all seen)
- 11. Taking `render()` apart (steps 1 to 4 done 2026-11-14; the rest, in its order, is in the section's "Next")
- 12. Android build (APK) and GDAL - notes (2026-10-30; **kept as a reminder only, no work planned**, the user 2026-11-16)
- 13. Derived severe-weather indices (SHIP, SIGTOR/STP) (**done**: segments 1 to 6, then moved into the model screen's charts: `ship` with the model's own hail size as lines, `stp` fixed layer for the HRRR, RRFS and MPAS runs, `stp_eff` / `scp` from SFCOA and RRFSD, both in the sounding indices; deferred by choice: effective-layer STP worked out from the HRRR/RRFS fields)
- 14. GFS charts: what is missing, and how anomalies could work (**complete** 2026-11-16: every row of the Tropical Tidbits table is a chart now)
- 15. History: an archive tier and one fetch path (the events screen and the history cursor built 2026-10-15 to 16; the archive tier and one fetch path not built)
- 16. REFS Ensemble Viewer (done 2026-10-01, see the section)
- 17. Sounding viewer: remaining SPC panels (**complete** 2026-11-16: every panel drawn, checked against SPC's OUN 12Z picture and text; one known difference noted, the advection column's lowest layer)
- 18. Tropical Hub (built 2026-10-08: storm cards, outlook, recon plan, season tallies; its model screens open on a selection since 11-10)
- 19. The polar vortex: initial states since a date (built 2026-11-14 to 15; home tiles 11-16; the analysis models' own levels 2026-11-19: 10, 30 and 50 mb for GFS-ANL, GFS1-ANL and CFS-ANL, drawn for 2009, 2013 and now)
- 20. Servers in one place (finished 2026-11-15; left: "Copy as override file", the run menu's note, overrides without a restart)
- 21. The product catalogue as data (chart files, the Catalogue page and "Make a chart" built 2026-11-15)
- 22. Ensemble percentiles at a place (built 2026-11-15)
- 23. Model comparison at a place (built 2026-11-15: bands, members, boxes, all models, bias, evolution; 2026-11-16: the Why? maps at the place, a station the person picks; left: wind and other variables at stations, the ensemble means for the bias)
- 24. The RRFS aerosols, and what else the files had (finished 2026-11-16: smoke and dust, emissions, haze, runoff, PLPL, ceiling, cloud by height on 26 models; seen on the map; checked against AirNow on a clean day, a smoky day still to try)
- 25. The Gross to Miserable weather index (built 2026-11-16; combined with the nice day 2026-11-18; the day boxes, the 6 hourly hours, the 12Z run for day 1 after 12Z)
- 26. IFS precipitation by type (2026-11-17)
- 27. The drought window's temperature and rain maps drawn from the data, zoomable like the Monitor's map (planned 2026-11-17: the CPC picture of the Normals tab to be replaced)
- 28. The sheet: a template of panels over one view (sketch 2026-11-17: sheet from a view, the template, the cases to check; nothing built)
- 29. The pictures of the drought window and the climate window that are still flat rasters, to zoomable layers (sketch 2026-11-18; the drought outlooks and EDDI built 2026-11-18; the SPI question and the climate window's layers left)
- 30. What is done and what is pending since 2026-11-18 (written 2026-11-19: the 0.2.1 run, the zlib and netcdf-c overlays, the help pages, small home items)

**Next (2026-11-16), in this order:**
1. (Section 24: done 2026-11-16; on a smoky day, repeat the AirNow comparison.)
2. (Section 10: complete 2026-11-16.)
3. The nice weather outlook: **skipped for now** (the user, 2026-11-16). His day 1-7 files of 16 Nov and our samples at his cities are kept as `<the kept-data folder>/2026-11-16.tar.zst` (zstd; outside the repository; unpack when the raw files are needed) to compare with the next days'. Open when it comes back: one rain rule for every day (the NBM 12 h daytime chance), a daily collector.
4. Section 23's left items, then section 8's.
5. **(2026-11-17, the user: "I must have seen the SPC. Let's do the same chart for models")** The SPC picture (CAPE filled, CIN as dashed lines every 100 J/kg) exists for the HRRR, RRFS and MPAS as `sfc_cape_cin`; now also a GFS-set chart of the same id (`GfsProductsGfs.cpp`, cloned wherever CAPE and CIN at the surface are carried: 19 models, the three families above keep their own). Tests green, not yet seen on a map. The masked version below (CAPE hidden under a strong cap) is still an idea, not started.
   **Seen (2026-11-17):** the GFS 24 hour chart drew correctly (CAPE filled, CIN dashed; in November the CIN's 0 line is noisy over dry air, as on the HRRR's).
   **A chart drawn by the program itself (2026-11-17, the user's idea):** `WXQT_OPEN=chart:<model>:<chart id>:<hour>:<out.png>[:<sector>[:<cycle yyyyMMddHH>]] QT_QPA_PLATFORM=offscreen build/release/wxqt` draws one model chart with the program's own settings, saves it, prints `MAGCHART ... ok|FAILED <error>` and exits 0 or 1 (2.5 s for the GFS). It takes the place of `tests/gfs/demo.cpp` / `demo.sh` (a second program that crashes on the app's settings): done: the one caller of `demo.sh`, `resourceCreation/mag/compare.py`, now calls it (tried on GFS/precip_p24). Left: `demo.cpp` / `demo.sh` stay until the command takes the demo's other options (`DEMO_OVERLAYS` ticked charts, `DEMO_MEMBER`, `DEMO_PALETTE`, `DEMO_MAG=0` own colour scales, `DEMO_REPEAT` timing), then they can be deleted (the test scripts only use `build_with_app.sh`, which stays: it builds a test against the program's objects, a different job).
   **The model's area (2026-11-17, the user: "RRFS-Fire needs a darker outline on the mesoscale area, and an option to zoom only to the area")**: (1) on the small-area models only (a warp box under 1000 square degrees, `GfsChart::regionalModel`: the fire nest, the Hawaii and Puerto Rico RRFS, the RTMA and URMA of Hawaii, Puerto Rico and Guam; the user: not needed on the full CONUS or global models, nor on the CONUS-sized HRRR and RRFS) a dark line is drawn round the data where it stops short of the frame (`outlineData` in `GfsChart.cpp`: the convex hull of the edge of the fill's valid pixels, so a field empty in places, the ceiling under a clear sky, still shows the whole box; a hole inside the data is not an edge). Checked on nine fire-nest charts (fill, lines, barbs, precipitation) and the HRRR; the model screen's Map menu has "Outline the model's area" (pref `MODEL_OUTLINE_AREA`, on); (2) a sector `AREA` second in the list of those models (group "The model's own area"): the box the data of the first field fills plus 4%, found after the load (the fire nest moves, so its box is not fixed). Seen on RRFS-FIREWX and RRFS-HI; left: the menu entry and the sector in the picker not yet seen in the window, the outline on the master map layer (it draws the grid itself), and a per-model choice if some model's edge should not be outlined.
   **Dropped (the user, 2026-11-17, after a try):** CAPE masked by CIN. It was built (CAPE left out where the CIN is under -50 J/kg, surface-based, mixed-layer and most unstable) and seen on a June 2026 RAP analysis, then removed: a snapshot mask hides CAPE the afternoon sun will uncap, hides elevated storms over a surface cap, rests on a CIN that is a poor predictor of initiation (the NWS Dodge City note), and the -50 is the STP's ramp, not a threshold. The CAPE with the CIN as lines (the SPC picture) stays. If it comes back: fade by the STP's CIN term instead of hiding, or a daytime maximum of the uncapped CAPE.
5. A first build of `f1ae9b5` on GitHub when wanted (not built yet: the last green build is `ae807f2`).

**The older, larger sections (11 to 18), what is left in each:**
- 11. `render()` apart: the remaining steps in the section's own "Next" list (one commit each, the golden test green after each).
- 12. Android: a reminder only (the user, 2026-11-16: keep the notes, no future work).
- 13. Severe indices: done (the dual contour with the model's hail size is in the `ship` chart). Only effective-layer STP from the HRRR/RRFS fields themselves was deferred, by choice.
- 14. GFS charts against Tropical Tidbits' menu: complete (2026-11-16).
- 15. History: the archive tier (keep a run, a loop or a day for research) and the one fetch path; the screen and cursor exist.
- 16. REFS viewer: done.
- 17. Sounding panels: complete (2026-11-16). Known difference, kept as a note: the inferred advection column's lowest layer.
- 18. Tropical Hub: built; the idea's later parts (the ocean conditions round a storm, historical seasons by basin) not checked against it.

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

## 8. Extended point view: a saved place's 11-day forecast from the NBM text bulletins (built 2026-11-12: `c4650a7`, then `7c8e5ea`, `f79aa2c`; what is left is at the end)

**Built (2026-11-12), read from the history on 2026-11-15, not re-run:** the Extended tab of the forecast-point page (`misc/ForecastPointExtended.*`, 512 lines) over `misc/NbmText.*` (432 lines: the bulletin parser and the one-station finder over range reads), the NWS grid for the first hours (`UtilityForecastPoint`), the merged table, the graphs and rows, and the hand-off (a picked time opens the 500 mb heights and winds on the newest SPC REFS run that has it, else the GFS; the Model Viewer opens a future valid time at the hour of the newest run that lands on it, `f79aa2c`). Headless: `WXQT_OPEN=nbmpoint:` and `extpoint:`. Steps 1 to 6 below are in; the pieces are fewer files than planned (the parser, finder, merge and tab are in two). **Left:** step 1's tests on real bulletin blocks and step 2's every-station check (none is in `tests/`: no test reads `NbmText`; the plan asked for them because the stand-in passed while 41 stations failed); mph; precipitation type as colours; the map thumbnail; NBP and NBH; the 'which run did it use' line; whether the place's station is kept per saved place (not checked). **The source of the idea:** `extended-point-view.md`, the summary of the "Extended" page of simplemodelview (Kotlin, checked 2026-11-12 on the live bulletins; Norman OK end to end, the station finder on every station of the 12Z NBX and NBE files). It is not a new data source for the app (the NBM GRIB model reads the same bucket) but the **long-range half of the forecast-point page** plus a hand-off to the Model Viewer.

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

## 9. Charts as presets of the checks (plan, 2026-11-13; the Checks picker and the alias table are built, 2026-11-13 to 15)

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



## 10. One model's field over another model's chart (built 2026-11-15 on the picture and the map layer; see the notes for what is left)

Asked while drawing §9: a SPC REFS product (a probability, a paintball, an ensemble max, the updraft helicity) drawn over an upper-air chart (500 mb heights and barbs) from another model. Out of the scope of §9, which keeps every picker inside one model, but §9 leaves the room for it. **Read from the code on 2026-11-15 (not run): most of the questions below are already answered by what the program does today, and the real work is smaller than the first guess and in other places.**

**What the program already does, and what it settles** (`GfsRender.cpp`, `GfsChart.cpp`, `GfsGrid.h`):
- **The grids need no resampling.** Every model's field is a `GfsGrid::Grid` on a regular latitude-longitude grid (the REFS Zarr store included: the custom loader hands the same kind of grid over, `kind = custom`), keyed by a name in `Grids` (a `std::map<string, Grid>`). `paintOverlays` already samples a grid by the longitude and latitude of each pixel of the view (`Grid::sample`, bilinear, NaN off the edge of a regional grid). A REFS grid of 3 km over a GFS chart of 0.25 degrees is drawn correctly with no code for the difference of spacing; off the REFS's edge the value is NaN and nothing is drawn. The question of "its own projection" does not arise.
- **The matching of run and time is written.** `addOverlays` (the other models' lines over a chart, `Variant::overlayModels`) takes the base chart's valid time, steps the other model's newest run back to its cycle at or before it (up to three cycles back, `cycleHours`), asks whether that run offers the hour (the model's `hours` segments and steps), and loads that run's grids (`gfs.load`, the same loader for every model, SPCREFS included). `Variant::Difference` does the same for a chart minus the other model's. `Variant::validTime` makes every model's hour come from the clock. So "which run" is: the newest at or before the base's time that has the hour; "which time of a window": the model's own hour, since a windowed REFS chart (`qpf_06h_acc`, the 4 h maximum) is the window ending at its hour, and `GfsChart::needs(product, hour)` already asks for that window.
- **A ticked fill is already a field over the chart.** The Checks picker's second fill is `Product::overlays`, a list of (grid key, ramp), painted translucent by `paintOverlays` with a "from" value (clear under it), its own scale (`asFillOverlay`, the Colors editor) and a legend row. It is made from records of the base model (`o_<id>_` names in `compose()`); nothing in `paintOverlays` or the legend cares whose records they are.
- **The REFS variant picker is the Checks picker.** The first and second steps of the old plan (parse the REFS ids into product, window, statistic and limits; the variant picker on REFS alone) are done by the alias table (`ChartAlias`, the "SPC REFS product grid" family) and the Hour / Type / Limit lists of `ChecksPicker`: 168 REFS ids, 96% of all chart entries read; the ids that did not read are the fixed-definition chances and the two-limit ones listed as one-offs.

**What is missing, in the order it would be met:**
1. **The chart of the second model is a whole `Product`, not a record.** `addOverlays` needs the *same chart id* on the other model (`GfsChart::product(chart.id, other)`) and draws its *lines* only, and a failure (no run, no hour) fails the whole picture ("X has no forecast for ... "). A cross-model fill needs: a (model, chart id) pair, its `needs` and `derive` and `fill` run on its own grids (as `addOverlays` already runs `product->derive` with the other model's `Context`), the resulting fill grid put in the base chart's `Grids` under a key of its own (`x1_fill`), its ramp (`defaultRamp`, then the user's palette, then `asFillOverlay`) in `product.overlays`, and its lines or barbs optionally the same way (a REFS chart has height lines of its own).
2. **A soft failure.** The second layer's trouble is not the base's: REFS runs to hour 60 and a GFS chart is often asked for day 5. The picture is drawn without it and says so (a line in the header: "SPC REFS: no run has this time"), where today a missing hour is an error. This is the main behaviour to decide (below).
3. **The legend, title and hover.** The title counts it ("... with SPCREFS Updraft Helicity 2-5 km (fill)", as the lines are counted); the legend gets a row named for the model and chart with its own scale; `Probe::fields` (what a click reads) gets the second field named for its model. The probe is built in `fillProbe`; the order the fields are read is tested by `run_probe.sh`.
4. **The key.** `ChartSelection` already carries `overlayModels` (a list, in `key()` and `fromKey`), and the Model Viewer's cache key, read-ahead and saved views are built from it (`ModelViewer.cpp` lines near 825, 1042, 2664, 3907). A `crossFill` entry (`model:chart id`) added there is what keeps the cache from showing one picture for two choices, and what saved views, favorites and the compare tiles (`sideBySide`'s `Panel`) carry.
5. **The picker.** One button on the Checks picker ("Another model's field...") opening the second model's own chart or checks picker (the same code, run on that model, returning an id) and a "clear under" value (the `FillFrom` of the ticked fills: they need a (model, id) key, not only the program's overlay ids). Its scale is the Colors editor's, by ramp.
6. **The master map is a different path.** The map layer asks for the chart through `Variant::fillOut`, `contoursOut`, `windUOut` (the grids handed to `WorldRender`, which draws the globe itself): a field added in `Grids` is not in them. There are two ways: extend the hand-over (a list of extra fills with their ramps and names, and `WorldRender::renderWorld` painting them as it paints the first), or have a **second `ModelChartLayer`** (the master map's whole idea is stacked layers, each with its own time, strength, legend and hover; REFS updraft helicity translucent over the GFS 500 mb lines and barbs would then be two layers with nothing new to draw). The second is the cleaner product but `"model/chart"` is one id used by about a dozen places in `MasterMapViewer.cpp` (`modelLayer()`, the exports, the history, the chart's context menu, the views): they would need to ask "a model chart layer", not "the one". Neither has been tried; the Model Viewer's picture comes first and the map after.

**Decisions to take before writing** (the first two change what is built): (a) **what a missing time does: decided 2026-11-15 by the user: draw the base chart without it and say so in the title** (`[SPCREFS: no field for this time]`; the reason goes to the log); (b) **one second field or a list** (one, to begin: the tint and the scale are readable for two fills, not for four; `overlayModels` already allows four more models' lines); (c) **whether the second chart's lines and barbs come with its fill** (no: the fill only, and the second chart's lines through the existing "other models' lines" when the chart has the same id; a REFS chart's own height lines would clutter); (d) **where the choice lives on the map** (the second layer or the extra hand-over).

**Steps, each with a test that needs no network** (a golden picture of two made-up grids of different spacing, the second regional, one with a NaN edge; the key round trip; the header line when the second layer is missing; the probe's field order): (1) a cross fill in `Grids` and `product.overlays`, drawn by `paintOverlays`, from a made-up second grid: golden picture `cross_fill`; (2) `GfsRender` loads the second model's product at the matching run and hour (`addOverlays`' matching code lifted into a function both use, `matchRun(model, validTime)`) with the soft failure; (3) `ChartSelection` key, the Model Viewer cache key and read-ahead; (4) the legend row, the title and the probe; (5) the picker button; (6) the master map (the second layer or the hand-over); (7) the world export and the history dialog follow what (6) chose. About two days for steps 1 to 5 (the picture and the Model Viewer), most of it in 2 and 3; step 6 is another two or three. The risk is not the drawing but the Model Viewer's read-ahead and cache, which fetch by key and must not fetch the second model for every hour of a loop it has no hour for.

**Built, steps 1 and 2 (2026-11-15):** (1) a golden picture `cross_fill` (a regional grid ten times finer than the chart's, with its edge and a gap, drawn by `paintOverlays` as it is: no change to the drawing was needed); (2) `GfsRender::Variant::crossModel` / `crossChart` and `addCrossFill`: the matching of the other model's run and hour is lifted out of `addOverlays` into `loadMatching` (a third copy of this matching is in the Difference variant and a fourth, `addExtras`, in `Product::extras`, which also resamples onto the chart's grid: left as they are), the second chart's `derive` and `fill` run on its own grids, its default scale made translucent and clear under its lowest sixth, put in the grids under `x1_fill|<model> <cycle>Z: <title>` (the key the legend and the read-out already take: a legend row and a probe field come with it), the title gains "with <model> <chart> (fill)". A time the model lacks leaves the chart as it is: `[SPCREFS: no field for this time]`. Headless: `WXQT_CROSS=<model>:<chart id>` with `chartpng:` (`WXQT_CROSS=SPCREFS:cref_max chartpng:GFS:500_wnd_ht:12:CONUS:out.png`: the REFS ensemble maximum composite reflectivity over the GFS 500 mb heights, barbs and lines, named in the title and the legend, clear off the REFS grid; hour 96: the GFS chart with the warning). **Steps 3 and 5 built (2026-11-15):** `ChartSelection::crossFill` ("model:chart id", the ninth field of the line a saved view keeps: a line from before it still reads; asked of the renderer for a plain chart only: a change or a maximum has no second fill), the Model Viewer's frame key (`|cross=...`, so the cache and the read-ahead keep one picture for each choice) and an "Another model's field..." button next to "Overlay models..." (its text becomes "Field: SPCREFS Composite Reflectivity (ensemble maximum)...") opening `ChartPickers::chooseCrossFill` (a model, a search of its charts that have a fill, "No field"); `tests/gfs/features_test.cpp` checks the line's round trip, the variant and the old line; a dev route `WXQT_OPEN=crosspicker:<out.png>[:<model>]` saves the dialog. Seen: the button in the Model Viewer and the dialog on the SPCREFS list; **not tried by hand:** picking a chart in the dialog and watching the viewer redraw, a loop, the read-ahead with a time the second model lacks (it may fetch for every hour: the risk named above), Save as a loop. **The common choice (2026-11-15, the user: the SPC's processed REFS 4-hour thunder is the most likely use):** the dialog has a "Common choices..." list (SPCPOST `thunder_4hr`, `thunder_1hr`, `thunder_full`, the REFS ensemble-maximum composite reflectivity and 4-h updraft helicity) and opens on `thunder_4hr` when nothing is chosen; the cross fill's alpha is 215. Drawn over the GFS 500 mb heights and winds (`WXQT_CROSS=SPCPOST:thunder_4hr`): the thunder probability reads as green over the Gulf, but the chart's own wind-speed fill is a competing blue: a base without a heavy fill (the heights alone) would show it better; **the fade is built (2026-11-15):** a "Fade GFS's own fill so this one reads" box in the dialog (on the first time), `Options::fillFade` (the base ramp's alpha times 0.3, applied in `prepare`; its legend fades with it), `Variant::fadeFill`, the tenth field of the saved line (`|fade` in the frame key), `WXQT_CROSS_FADE=1` headless: the thunder probability then reads over the Gulf with the heights and barbs unchanged. Not yet: steps 4 (partly: the legend row and the read-out come with the key, seen on the picture, not clicked), 6 and 7 (the map layer, the world export and the history dialog do not draw the second field), the "from" value (a sixth of the scale is cleared whatever the field), a second field in the compare tiles; the title can be cut by the header's width with a long chart name.

**Questions the first draft asked, answered by the reading above:** the run to use: the newest at or before the base's time that has the hour (written, `addOverlays`); the time of a windowed field: the model's own hour (the window ends there); resampling: none (per-pixel sampling of regular grids); which model's timeline the slider follows: the base chart's (the second goes on the clock, as the lines of other models do now); naming in the legend and hover: step 4.

**Why it looked like more than a small addition** (the first guess, 2026-11-13, before reading the code; the answers are above): today the ticked fills are fields of the same model and run as the base chart (`compose()` with the `o_<id>_` prefix), so they share one grid, one run and one valid time. A field from another model has its own grid (REFS vs GFS/ECMWF), its own run, and often a window ("4-h max") where the base chart is an instant. Open questions: which run of the second model matches the base chart's time (latest, or the one valid at that time); which time of a windowed field is the chart's (the window ending at the valid time); resampling the second field to the base grid, or drawing each in its own projection; the legend and hover naming the model of each layer; and which model's timeline the slider follows.

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
| contour numbers (`drawContours`) | one per line longer than 150 px, at 50% along it; at 50% along it, else the first of 40, 60, 30, 70, 20, 80% that is free | other contour numbers (their boxes in `placed`), the frame (24 / 14 px in); not the barbs (the background) | H and L, point values, the marker and track labels |
| highs and lows | at the extreme (radius 12, 8 or 6 by the view's width), the value 13 px under | only the frame (20 px in) | everything else, and everything after them avoids nothing of theirs |
| wind barbs (`drawBarbs`) | a grid of pixels (`BarbGrid::look`), the first set longest | nothing: the others avoid them | all labels; they are drawn over the H and L |
| point values | cities biggest first, then an even grid, 120 / 96 / 80 px apart | each other (their own `boxes`), the frame (4 px in) | the contour numbers, H and L, barbs: a translucent box is drawn over them |
| marker name, track labels | a fixed offset from the point (10, -8 and 9, -7) | nothing | everything |
| frame marks, header, legend | fixed places; the header and the line under it shrink, the legend's keys run left to right | the header shrinks to 11 / 9 px and is cut with an ellipsis; the key of the sets does not wrap | a key can run past the right edge (not checked) |

Found in it: (1) the width of a number's box is guessed (`text.size() * 8`), where the point values use the font's real metrics; (2) the barb grid *rule* is one (`BarbGrid::look` and `lonStep`, used by every map and chart: that part is right), but the walk over its points is written out in `drawBarbs`, `paintBarbs` and `paintStackedBarbs`, and the clearance of the numbers worked out its nearest node by hand: that is now `BarbGrid::nearestNode` (`tests/gfs/run_barbgrid.sh`); the walk over its points is now `BarbGrid::forEachNode` / `forEachWorldNode`, used by `drawBarbs`, the two loops of `ModelChartLayer` and the globe's in `WorldRender` (the test compares it, point for point, with the loops it replaced; the map layer and the globe have no picture test of their own, and `SarWindsLayer` walks a different, cell grid and is left); (3) in the map layer the numbers are vectors placed at the picture's size, so their clearance from the barbs is that of the picture's barb grid, while the layer draws its barbs for the zoom on view: the two grids can differ, so a number can end under a barb there although the picture is clean; (4) the inset from the frame is a different constant for each kind (24 / 14, 20, 4); (5) no kind knows about the H and L, so a number or a point value can sit on one. **Fixed so far (2026-11-14):** the numbers on the lines and the point values now keep clear of the highs and lows, and the point values of the numbers on the lines (`drawContours` returns the boxes it placed, the point values start from them); `Options::placedOut` hands out every reserved box with its kind, and `tests/gfs/run_placement.sh` checks that no two overlap and each is inside its frame margin, on three widths and a dense chart, and on points put on the very places of the numbers (the test fails with either change taken out). The box of a number is now the width of its text as drawn (the font's metrics, not 8 px a character), and the highs and lows of every set are registered before any line is numbered (a height number was landing on a pressure high when the heights came first). The name of the marker (the ring on an area round a saved location) takes the first free side (right and above, right and below, then left) and records its box (the test fails with only the first side). **Decided 2026-11-15: the barbs are the background.** They are drawn first (the picture, the map layer and the world export), under the lines and under every number, and nothing moves for them: the numbers on the lines are put at the middle of their line (the first free of the other numbers, no longer the first clear of a barb node: `nearBarb` is gone), and the point values never avoided them. The goldens `lines_barbs` and `scaled` were remade (the barbs under the lines). **Track labels (2026-11-15).** The label of a storm track point ("+24 h  90 kt  970 mb") was centred 9 px from its dot, on it, and placed nothing; it now sits beside the dot (right and above, right and below, left and above, left and below, then past a number standing on the point, then higher), the first place inside the frame margin and clear of every box already placed (the numbers on the lines, point values, highs and lows, the earlier labels and the track's key, whose place is taken first), records its box as kind "track" (the key as "track key"), and the marker's name now keeps clear of them too. `tests/gfs/run_placement.sh` puts four track points on the very places of four numbers and checks no label is over a number, another label or the key. Still open: the map layer's vector numbers are placed at the picture's size (now only against each other, so it no longer matters that the barb grids differ; the track is in the picture only).

**Related: the maximum (and minimum) point value of a field (2026-11-14). First part built: a "Mark the maximum" toggle in the Map menu (pref `MODEL_MAX_MARK`, off by default, like "Mark the place") draws a ring and the number at the place of the largest value the fill shows, the one the legend's "max" tick gives (`drawMaxMark`, `FillResult::maxAt`; a golden picture `max_mark`). Not yet: the minimum, the local extremes for any field, the placer's avoidance of the barbs and numbers (the number only flips side near the frame), the map layer (the mark is in the picture only).** Today only a contour set with `highsAndLows` (the pressure, one height overlay: three places in the catalogue) marks its extremes, as H and L with the value under. The legend already marks the largest and smallest value on view (the red tick; `fillMax`, `fillMin`) but not where they are. The same annotation for any field (a temperature, a pressure, a height, a dewpoint) is one kind with two scopes: *local* (every extreme, as H and L now: "H 1032") and *the one* (the single largest and smallest on view, a small marker and the number: "104", "-12"). It belongs in the placer as a kind of its own, with the barbs as obstacles and its priority above the contour numbers (it is the thing read first); the number is in the user's units (`shown(quantity, v, us)`, as the point values do). It needs: the location of the extreme of the fill (`paintFill` keeps only the value: it would keep the pixel too), a per-chart or per-tick option (`Product::extremes`: none, local, one), the existing `GfsGrid::extremes` for the local scope, and, for the map layer, the same two labels as vector labels (they exist for H and L already). The picker side is a tick on the checks (a family "Marks": "max/min value", "highs and lows"), saved with a preset like the other ticks.

**What the placer would take over:** a list of things that want a place (kind, priority, the places to try, the box from real metrics), the barbs first as fixed obstacles (the grid in one function both the picture and the layer call), then the H and L, then the contour numbers, then the point values and the labels, each avoiding what is already placed; the same function for the picture and the map layer's vectors (placed again for the view on screen). The test for it is a picture with the text on and a crowd of everything (a line through every barb node, an H on a line, points of all kinds), checked not by pixels but by the boxes the placer returns: none overlaps another, every one inside the frame.

**Machine use, a thought for the same stretch:** the program is driven today by a number of separate `WXQT_*` variables that work only in odd places (`WXQT_DRAW` only inside the `magcheck:` route, `WXQT_CLICK` meaning two things). One documented command line (`render <model> <chart> <hour> --overlays ...`, `dump charts`) with fixed, JSON outputs, and one runner for `tests/gfs/*` that shows the stale ones (`run_models.sh` was stale and is brought up to date, 2026-11-15: the words about a model are now the Markdown pages in `help/models/`, so it checks each page it finds for the why and the data link and lists the ten models with no page yet (MRMS, WOOF-NAM-HOSTED, RRFS-FIREWX, the RTMA and URMA of Alaska, Hawaii, Puerto Rico and Guam); HRRR is a model; a clone needs 1 chart, not 10 (the surface-only regional models have 1 to 6). **Known gap it prints: AROME-HD has no chart** since its clone has no PRMSL, so none of the GFS charts is cloned), would make the tests, the CSV tools of section 9 and any script read the same thing. Not decided.

## 12. Android build (APK) and GDAL - notes (2026-10-30)

**Sunset (2026-11-16, the user):** kept as a reminder; no work is planned on it.

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

**Updated 2026-11-16:** built: SHIP and STP (fixed and effective) are in `SoundingIndices` and are map charts (`ship`, `stp`) of the HRRR, HRRR-AK, RRFS and MPAS runs. The text below is the plan as written.

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

**Checked row by row (2026-11-16) against the GFS's 89 charts:** built since the table: the rain / frozen split (`precip_type`, `precip_rate_type`, the `*_type` thickness charts), 700-300 mb RH and the 2 m dewpoint, MSLP & PWAT, the MSLP 48 h trend and anomaly, the Z500 trend and 500 mb anomaly, SBCAPE with MSLP and wind, 700 mb temperature with MSLP, 850 mb vorticity and streamlines; **built today:** the 2 PVU dynamic tropopause, its pressure (`dt_pressure`) and its potential temperature (`dt_theta`) with the wind on it (each hemisphere its own surface, +2 or -2 PVU; GEOS-FP skips them, it has no such surface), and the **simulated IR** (`sim_ir`: the outgoing longwave at the top of the atmosphere as a brightness temperature, the mean of the last 3 to 6 hours, on an enhanced IR scale moved for this brightness: clear air is already -25 to 0 C). Seen over North America, 12Z +24 h. **Built later the same day:** the **850 mb temperature anomaly** (`850_temp_anom`, heights and wind; clear where the ground is above 850 mb), the **2 m temperature anomaly** (`2m_temp_anom`, against the reanalysis' daily mean near the surface, so the time of day is in it), the **precipitable water anomaly** (`pwat_anom`); each with the standard deviation view (the normalized anomaly), as the height anomalies have. The 2 m and PWAT ones are cloned onto the IFS, AIFS, AIGFS, GEFS, CFS and CFS-ANL (the CFS ones from the CFS's own normal); all drawn. **Section 14 complete (2026-11-16):** the rest built and drawn (GFS 12Z, +48 h): **snowfall at 10:1** (`snow_24h`, `snow_total`: 6 hour pieces of the precipitation times the percent frozen, CPOFP, at each piece's end), **temperature advection** and **frontogenesis** at 850 and 700 mb (`*_temp_adv` in C per 3 hours; `*_frontogenesis`, the 2-D Petterssen kinematic one, K per 100 km per 3 hours; clear under the ground), the **Okubo-Weiss number** at 850 mb with streamlines (`850_okubo_weiss`: strain against rotation, the wind smoothed to the synoptic scale: at 0.25 degrees the small eddies swamped it), **Q-vector divergence** at 300 mb (`300_qvector_div`: geostrophic wind from the heights, worked out on a 1 degree grid: at 0.25 degrees it was the grid's noise), the **200 mb velocity potential and irrotational wind** (`200_velocity_potential`: laplacian(chi) = divergence solved exactly on the globe, zonal wavenumbers and a tridiagonal system in latitude for each, `GfsGrid::velocityPotential`; the first try, relaxation, reached 63% of the answer; copied only to the 17 global models through the new `Product::cloneTo`), **surface CAPE with the 850-500 mb crossover** (`sbcape_crossover`: lines of the veering from 45 degrees where CAPE is 100 J/kg or more), **330 K isentropic PV** (in the isentropic list, the levels 850 to 100 mb, pressure lines every 25 mb) and **300 K / 290 K** relative humidity, wind and pressure (`300K_isen`, `290K_isen`, clear underground). `GfsGrid::gradient` and `velocityPotential` have a test (`tests/gfs/gfs_test.cpp`: a known gradient; chi = C sin(lat) given back to 0.005%). The copies on other models drawn: the IFS velocity potential and 330 K, GEOS-FP advection, ICON-EU Q-vectors, RRFS crossover.

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
**Updated 2026-11-16:** the History events screen (`EventsViewer`, 2026-10-15) and the history cursor (`HistoryCursor`: the master map and the Model Viewer follow the History page's time, 2026-10-16) are built; the archive tier and the one fetch path are not.

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

## 17. Sounding viewer: remaining SPC panels (**complete**, 2026-11-16)

**Done (checked 2026-11-16, `WXQT_OPEN=raobsound:OUN:2026111612`):** every panel below is drawn: the inferred advection column, theta-e with TEI, storm-relative winds with the classic-supercell envelope, the effective-layer STP and SHIP box plots, SARS (supercell and significant hail), and the SPC tables (MU parcel, K index, mid and low RH, ESP, MMP, WNDG, SigSvr, NCAPE, Corfidi, MPL), with the tornado-chance box and the possible hazard type besides. What follows is the plan as written.

**Known difference, noted (2026-11-16): the inferred temperature advection column's lowest layer.** On OUN 12Z of 16 Nov 2026 ours is +1.8 C/hr in the surface-to-890 mb layer where SPC has -0.1, from the same levels (the surface wind calm in both); the seven layers above agree with SPC to a few tenths (-0.1/-0.1, -0.9/-1.1, -0.7/-0.8, -0.1/-0.1, -0.8/-0.4, -0.0/-0.1, +0.3/+0.4). The port follows SHARPpy's `inferred_temp_adv` line by line; a calm wind is taken as 0 degrees as SHARPpy does (that did not change it). Likely SHARPpy's wind interpolation at the surface (it may skip a calm report or the levels with no wind) differs from ours; worth a look if a case with a calm surface wind and a stronger lowest-layer signal comes up. Not a blocker: the section is complete.

**Checked against SPC's own picture and text (2026-11-16, OUN 12Z, `spc.noaa.gov/exper/soundings/26111612_OBS/OUN.gif` and `.txt`; the same levels and values as our feed, the calm surface included):** the SB, ML and forecast-surface parcels, PW, K, DCAPE, DownT, WBZ, freezing level, MaxT, the mean mixing ratio, the shear, the BRN shear, the 4-6 km SR wind, the Corfidi vectors and the effective-STP probability lines all agree; SRH, mean winds and SR winds within 1-4 degrees or 1 unit. Done: the 850-500 mb lapse rate is in the table now (4.1, as SPC); the sounding test builds again (`tests/sounding/run.sh` lacked the SARS files) and its "3-6 km" check compares the sea level layer, which is what SPC's text product means (its 4.7 against the picture's "3-6 km AGL" 5.5): 518 comparisons over 13 soundings, all within tolerance. **Differences left, understood or small:** (1) SPC's picture prints the MU row as the surface parcel ("MU (990 mb)") while SPC's text lifts it from 562 mb (LCL 6409 m, LI 5); ours lifts from about 603 mb (LCL 5752 m, LI 5.6) because the search stops 400 mb over the surface (590 mb) as SPC says it does, and 562 mb is 428 mb up: not explained (SPC's text kept in the scratchpad, not a fixture, so CI stays green). (2) The advection column's lowest layer: +1.8 against SPC's -0.1 on the same calm surface wind (a calm wind is now 0 degrees, as SHARPpy's; that was not it); the other seven layers agree within a few tenths. (3) TEI 23 against 25, the steepest lapse rate 6.4 against 6.2, mid and low RH 64/95 against 66/96, Bunkers 1-2 degrees: the formulas are SHARPpy's; the interpolation or the theta-e routine differs in detail.

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

**Updated 2026-11-16:** built 2026-10-08 (`src/dashboard/TropicalHub.*`: storm cards, the outlook, the recon plan, season tallies); its model screens open on a `ChartSelection` since 2026-11-10. What is below is the idea as written.

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

## 19. The polar vortex: the initial state of the global models since a date (built 2026-11-14 to 15: history, strip, change, section, waves; see the notes below for what is left)

**The wish:** watch the stratospheric polar vortex through the season: the f000 (the initial state) of the global models, as a history from a chosen date to now.

**What is already there.** (1) Initial states: `GFS-ANL` (the GFS f000), `GFS1-ANL`, `CFS-ANL`, and ERA5 at NCAR (hourly, 37 levels, back to 1940, read through the `pl:` files: any "N mb" level of HGT, TMP, UGRD, VGRD works). (2) A chart for a valid time ("by the clock", `Variant::validTime`): the screen already asks for a model's run by the time it is valid. (3) A `POLAR` sector (55 to 82 N, as a Mercator rectangle) and, on the master map, the polar projections. **Checked on the NOAA archive (2026-11-14):** the GFS f000 file carries HGT, TMP, UGRD and VGRD at 1, 2, 3, 5, 7, 10, 20, 30, 50, 70 and 100 mb, and the archive on S3 reaches back at least to April 2021 (`gfs.20210401/00` answers). The ECMWF open data has 50 mb as its highest level.

**What is missing.** (1) No chart above 200 mb in the catalogue (nothing at 100, 70, 50, 30, 10 mb). (2) No loop over dates: the loop is over forecast hours of one run; a history is one frame per valid time (every 6 hours), each its own f000. (3) No numbers of the vortex: the winds at 10 mb and 60 N (the zonal mean: a sudden warming is when it turns to easterly), the temperature of the polar cap (60 to 90 N), the height at 10 mb of the pole.

**In steps, each useful alone** (the plan as first written; see the note under it for what turned out to exist): (1) the stratosphere charts: 100, 50, 30, 10 mb height, wind and temperature (the temperature filled, the heights as lines, the wind as barbs or streamlines), for the GFS and so for the models cloned from it, ERA5 and the analyses; on the polar projection of the master map, since a Mercator rectangle cuts a vortex across the pole into stripes. (2) A history player: a model that has an initial state, a start date and a step (6, 12, 24 hours): frames by valid time, looped and saved as an animation; `validTime` is the way in, the fetch is a box of the grid (a few levels' bytes), so a season is hundreds of small reads, not downloads of whole files. (3) A strip of numbers under the loop: the zonal mean wind at 60 N and 10 mb and the polar cap temperature, one point for each frame, with the line and its dates. (4) Perhaps: the vortex edge (the 10 mb heights' lines, or the potential vorticity), and the forecast on the same strip (the f000 history, then the model's forecast continuing it).

**Built (2026-11-14), step 1 in part:** wind-and-height and temperature-wind-and-height charts at 100, 50, 30 and 10 mb for the GFS (`GfsProductsGfs.cpp`, `stratosphereTemperature` ramp, heights every 6, 8, 12, 16 dam), and so for every model cloned from it that lists those levels (the clone filter does this: IFS, AIFS, ICON, ARPEGE and GDPS get 100 and 50 mb; GFS, GEFS and GEPS all four; ERA5 gets 30 mb too). They fold into the level lists of the Checks picker (the alias table is regenerated: `#_temp_ht` now has 100, 50, 30 and 10 beside 850 and 925). Drawn from live GFS data for the POLAR sector (`chartpng:GFS:10_temp_ht:0:POLAR`): the vortex is plain. **Found:** ERA5 at NCAR ends in March 2023, so it cannot show this season; the history has to be the GFS f000 (open data from 2021) and the other global models' own initial states. **The polar projection is already there:** the master map's "Export the whole world" has a north polar projection built for the polar vortex (`WXQT_OPEN=modelat:GFS:202611150000:10_temp_ht:POLAR WXQT_WORLD_EXPORT=<file>,north` draws it headless, a hook added today): the GFS 10 mb f000 of 15 November shows the vortex pushed toward Scandinavia and Eurasia, with the barbs and the height lines. **The history loop also already exists** on the master map: "Loop from Time 1 to Time 2" (`looprange:` route; `ModelChartLayer::setRange`) steps the model chart through past valid times by the model's cycle step (6 hours for the GFS, so each frame is the f000 of the run at that time) and the loop can be saved as a GIF or video (`saveModelLoop`). Not yet tried together on the stratosphere charts. **Step 2 built (2026-11-14), the history of initial states.** The finding above was that the master map's own range loop (`setRange`) is one run's forecast hours from the start of the range, not each run's f000. So: *Export... > "The model chart's initial states from a date, as a loop (the polar vortex)"* (`MasterMapViewer::saveInitialStates`): from and to (default 1 October of the season to now), every 6, 12 or 24 hours, north polar, south polar or Equal Earth, 800 to 1600 px; each time the model layer is put at it (`setAsOf`: the run at that time, hour 0) and the whole world exported quietly through `exportWorldTo` (`initialStateFrames`), then the loop saved as a GIF or video by the usual dialog. Headless: `WXQT_OPEN=history:GFS:10_temp_ht:2026111000:2026111200:24:<folder>:north` writes the pictures (three days checked: each titled with its own run, F0, and the vortex moving). Not tried in the dialog by hand; no automatic test (it reads the network); a season at four a day is some 180 pictures. **Step 3 built (2026-11-15), the strip of numbers, as an option.** A checkbox in that dialog ("Add the vortex strip under each picture"; headless `WXQT_HISTORY_STRIP=1`): the picture gets a band under it with the zonal-mean wind on the circle of 60 degrees (the first source's wind grid sampled every 2.5 degrees of longitude, in m/s; blue, with a dashed zero line, since a wind that turns below zero is the sudden warming) and, when the chart is a temperature one (`_temp_` in its id), the mean temperature beyond 60 degrees (every 2.5 degrees of latitude weighted by the cosine, in degrees C; red, its scale at the right). One point per picture, the whole season faint, the line to this picture bold with a ring at it, and the value in the words above (`withStrip`, `MasterMapViewer::vortexNumbers` set in `exportWorldTo` from the grids already loaded, so no extra download). South polar map: 60 S. Left out when the chart carries no wind. Checked on three GFS 10 mb frames (11-10 to 11-12): the wind falls from +0.8 to -3.4 m/s as the vortex is pushed off the pole; the strip's temperature scale is in degrees C while the legend is in the user's units; not tried in the dialog by hand. The zonal mean hides a split vortex (the mean can stay positive while the map shows two lows). **The change since an initial state (2026-11-15).** A "Show the change since the initial state of <date>" box in the same dialog (headless `WXQT_HISTORY_CHANGE=yyyyMMddHH`): each picture's fill minus the fill of the run at that date (read once, `changeBase`; each model's own grid, the earlier one sampled at each cell), on a diverging scale (white at no change; blue to red, 30 C either way on a temperature chart, half the chart's top value on others), its own legend row, the height lines and barbs of each picture kept. GFS 10 mb temperature 26 Oct to 15 Nov checked: by 15 Nov the Arctic from Canada to the pole is 15 to 20 C warmer, Scandinavia and Siberia 10 to 20 C colder. The strip stays the absolute numbers. Not tried in the dialog by hand; the wind charts' change scale is a guess. **The time-height section (2026-11-15), first part.** `src/mapkit/VortexSection.*` (no widgets): `collect` reads, for each time (the run at that time, hour 0) and each level, the chart `<level>_wnd_ht` through `WorldRender::loadSources` and takes the mean of the wind's u on the circle of 60 degrees (north or south; 2.5 degree steps); `draw` makes the picture: a row for each level (the rows are the levels, no interpolation across the gaps), a column for each time, red westerly and blue easterly on one scale, the dates under, a color bar. Headless: `WXQT_OPEN=section:GFS:<from>:<to>:<step h>:<out.png>[:north|south][:10,30,50,100,500,850]`. GFS 1 Oct to 15 Nov at 12 hours (91 times, six levels) took about seven minutes: the wind at 10 mb is strongest in late October (about 25 m/s), falls through November and turns slightly easterly around 10 Nov; the 30 and 50 mb winds weaken at the same time but stay westerly; 500 and 850 mb stay westerly all along. **Only levels that have a wind chart** (10, 30, 50, 100 and the lower ones: 200 to 300 mb have the barbs cleared, so no u): **the 70, 20, 5, 2 and 1 mb charts were added (2026-11-15)** (heights every 8, 12, 20, 24, 28 dam; the 5, 2 and 1 mb temperature charts use `upperStratosphereTemperature`, -80 to +10 C, since the 1 mb field is over the -20 C the other ramp tops out at and drew flat red; the alias table, the catalogue dump and the presets regenerated), so the section has its upper rows: 1 to 70 mb of 10 to 15 November drawn (the 1 and 2 mb winds are westerly and strengthening while 5 and 10 mb sit near zero or slightly easterly). **In the Export dialog (2026-11-15):** a kind "The polar vortex through the season: the wind by level and time (a picture)" beside "history" (`MasterMapViewer::saveVortexSection`: from, to, step 6/12/24 h, north or south; the reading runs in a thread behind a progress dialog with Cancel; the levels asked are 1 to 850 mb and those with no wind chart drop out; the picture saved through the file dialog). Built, not clicked through by hand. **Waves (2026-11-15):** `VortexSection::collectWaves/drawWaves`, headless `WXQT_OPEN=waves:<model>:<from>:<to>:<step h>:<out.png>[:north|south][:level]` (default 10 mb): the amplitude of wave 1 and wave 2 of the height on the circle of 60 degrees (a Fourier series of the height grid, every 2.5 degrees, in metres) with the zonal-mean wind beside them. GFS 10 mb, 1 Oct to 15 Nov: wave 1 climbs from about 100 to 780 m (the vortex pushed off the pole), wave 2 peaks near 2 Nov (about 600 m) and collapses to about 50 m by 11 Nov, while the wind falls from 25 m/s to slightly easterly. In the Export dialog as the "Picture" choice of the vortex section kind (with a level box; built, not clicked through by hand). **Potential vorticity (2026-11-15, the user: the vortex video seen for science communication showed it):** `GfsProductsGfs.cpp` makes `<level>_pv_ht` for 100, 70, 50, 30, 20 and 10 mb: Ertel PV on the pressure level in PVU, PV = -g (zeta + f) dtheta/dp from the temperature at the level and at the files' levels above and below (70/150, 50/100, 40/70, 20/40, 10/30, 7/20 mb) and the wind at the level (the relative vorticity on the pressure surface: the slope terms are left out), the southern hemisphere's sign turned over; heights as lines, barbs; `potentialVorticityRamp(top)` with a top for each level (40, 60, 90, 220, 360, 560 PVU: a quarter above the largest value of the 15 Nov 2026 00Z initial state). The GFS files carry PV only on the 2 PVU surface, so it is worked out. Checked: all six drawn (10 mb 458 PVU at most, 100 mb 32); **not checked against a reference** (ERA5 or the GFS 2 PVU surface), drawn on the polar map for 1 to 15 Nov (a video: the vortex two-lobed on 1 Nov, the Eurasian lobe dominant on 8 Nov, weak and off the pole on 15 Nov, matching wave 2 peaking near 2 Nov); speckle at grid scale and a white dot at the pole (no vorticity on the pole's row) are not fixed. **The IFS 0.1 degree has it too (2026-11-15):** its files carry 10 and 50 mb above the 100 (no 7 or 20), so `IFS` gets `10_pv_ht` (`ifsRecipes` in `GfsChart.cpp`: the one-sided 10-50 mb difference, `potentialVorticityOf(10, 50)` now in `GfsChartInternal.h`, about a third of the GFS's values: same pattern, not the same numbers; scale top 260 PVU) and `10 mb` is in the IFS's `clone_levels` (its 10 mb wind and temperature charts are cloned too; the runs before 2026-10-20 are read from the 0.25 degree file, which has no 10 mb). Not in the other model clones. Not built: PV on an isentropic surface (850 K, 530 K: what science-communication videos usually show), scaled PV (Lait). **PV, corrected, isentropic, and GEOS-FP (2026-11-15, later).** (1) **The stability is now in log pressure:** dtheta/dp = (dtheta/dln p) / p (`potentialVorticityOf(pAbove, pLevel, pBelow)`), because the plain difference over a wide stratospheric layer is the gradient at its middle and theta rises like 1/p. At 10 mb on 15 Nov 2026 00Z: GFS 458 -> 568 PVU, the IFS 10-50 mb layer 234 -> 583, GEOS-FP's own EPV 718; the other levels move under 10%. The scale tops were retuned (GFS 10 mb 710, IFS 740, GEOS the GFS top x 1.27). (2) **Isentropic PV** (`GfsProductsGfs.cpp`, GFS only): `850K_pv`, `650K_pv`, `600K_pv`, `530K_pv`, `500K_pv`: the pressure-level PV at the levels round the surface, the pressure where theta crosses theta0 found in each column (linear in theta against ln p), and the PV, the wind and the pressure put on it; lines are the surface's pressure, barbs its wind. Levels used for each: 850 K 3-30 mb, 650 K 7-50, 600 K 7-70, 530 K and 500 K 20-100; a column where theta0 falls outside the levels that have a PV is left clear (600 K needed the 7 mb level for the 15 Nov core). Scale tops 900, 400, 250, 130, 70 PVU from one frame each. 1 to 15 Nov 850 K video: the two-lobed vortex of 1 Nov, a long streamer drawn out of the Eurasian lobe on 8 Nov, one oval over Eurasia on 15 Nov; 600 K shows two near-separate vortices on 8 Nov, rejoined by 15 Nov. Not checked against a reference; the ripples are in the data. (3) **GEOS-FP's 3 hourly pressure-level collection** (`inst3_3d_asm_Np`: 42 levels 1000 to 0.1 hPa, H, T, U, V, QV, OMEGA and **EPV**, 1152 x 721, one level a block so a range read costs one block): a model file's `nc_levels` (the file's level order) and an `@lev` suffix on an `nc_variable` name make the band of the want's level (`GfsModelFile.cpp`); the four levels of the hourly collection stay on it, every other level is read from this one (`valid_offset_file = inst3_3d_asm_Np:0`); `value_scale = EPV:1000000` gives PVU; 925, 700, 300, 200, 100, 70, 50, 30, 20, 10, 5, 2 and 1 mb were added to its cloned fields, and its `<level>_pv_ht` charts read EPV itself (`geosPv` in `GfsChart.cpp`). A frame of the polar history takes about a minute (four levels of range reads and the warp to a 0.25 degree grid); the 3 hourly files only exist at multiples of 3 hours. GEOS frames show white wedges near the right edge of the polar map (8 Nov, 1 Nov): not understood. `tests/gfs/modelfile_golden.txt` was remade (it had drifted: ERA5 30 mb, the IFS label and lag; and `scale=1` on GEOS wants). **The white disc at the pole of the PV maps (2026-11-15)** was in the data, not the projection, and the winds are known to the pole: `GfsGrid::vorticity` (a latitude-longitude formula dividing by cos latitude) leaves every row poleward of about 89 degrees clear (rows 0 to 4 of a 0.25 degree grid), so a disc of about 1.25 degrees had no value (the temperature maps had none). **Fixed with another projection:** for a global grid that reaches the poles, `GfsGrid::vorticity` now works the two polar caps (86 degrees to the pole) on a polar stereographic grid centred on the pole (a conformal map, no singularity; scale k = 2 / (1 + sin latitude)): the winds are sampled onto it, turned to the map's axes, the vorticity is k^2 (d(Vy/k)/dx - d(Vx/k)/dy), and it is read back onto the latitude rows from 86 degrees to the pole (the south cap mirrored, the frame kept right-handed). Every vorticity chart gains it, not only the PV. `tests/gfs/features_test.cpp` (`poleVorticity`) checks a solid-body rotation (u = U0 cos latitude: vorticity exactly 2 U0 sin(latitude) / R, the poles included) to 3% at 15 latitudes from 90 N to 90 S and the pole row's equality: 21 failures without the caps, none with. The white blob a few degrees from the pole on 8 Nov is real (the PV there is about zero or below and the scale starts at 0). The polar videos made before this still show the disc. **The section's scale, by row (2026-11-15):** `Series::perRow` (set by `collect`, not by the time-latitude section): each level's row is coloured by its own largest wind (steps of 5 m/s under 30, of 10 above, at least 5) with that scale written at the right of the row (`+/-30` at 1 mb, `+/-10` at 500 mb ...), the colour bar labelled "most easterly / 0 / most westerly"; a box in the Export dialog ("Scale each level's row by its own largest wind", on) and `WXQT_SECTION_SCALE=global` for the old single scale. 1 to 15 Nov: the 5 and 10 mb winds turn weakly easterly about 8 to 12 Nov and the 20 mb nearly stops, which the single scale (set by the 1 mb winds) hid. **Time-latitude section (2026-11-15):** `VortexSection::collectLatitudes` (the zonal-mean wind at one level every 5 degrees from 25 to 85, pole at the top; one wind read per time), headless `section:<model>:<from>:<to>:<step>:<out.png>[:north|south]:lat10`, and the third "Picture" choice of the dialog (with the level box). GFS 10 mb, 15 Oct to 15 Nov: the winds turn easterly first at the pole (about 1 Nov) and the easterly spreads down to about 65 N by 5 to 8 Nov, while 35 to 45 N gets stronger (40 N over 30 m/s by 15 Nov); 60 N is only near zero. **The strip's temperature now follows the user's units** (degrees F with the legend; `UIPreferences::unitsF`). **Wind-chart change scale checked:** 10 mb wind change since 1 Nov, 13 to 15 Nov, drawn with the half-top-stop rule (about 100 kt either way): the jet's gain over Europe saturates at the red end, the rest reads. Not yet: the strip's second line; none of the dialogs clicked through by hand. **Done in step 1 (2026-11-19, the user: "Analysis models 19? ... Yep. Let's look"):** the analysis models' own level lists. The catalogue check of each model's newest files (`catalogcheck:`, its "not cataloged" list) showed HGT, TMP, UGRD and VGRD at 10, 20, 30, 50 and 70 mb in the NCEI GFS-ANL (0.5 degree, 2007 to mid 2014) and GFS1-ANL (1 degree, GRIB1) and to 1 mb in the CDAS (CFS-ANL); the clone levels of the two NCEI ones now include 10, 30 and 50 mb and the CFS-ANL's field list 100, 50, 30 and 10 mb of the four variables. Drawn on the POLAR sector: 10 mb wind and height of 24 Jan 2009 (GFS1-ANL), 6 Jan 2013 (GFS-ANL) and 19 Nov 2026 (CFS-ANL). Not done: the 20 and 70 mb levels (the main GFS lists none of them either), the CFSR of 1979 to 2010 (NCEI's monthly files, no index: not read yet).
**The Model Viewer's polar areas (2026-11-19, the user: "Model viewer polar projection?" ... "Yep that's perfect"):** two areas in the list, "North polar view (the pole to 30 N)" (`NPOLE`) and "South polar view" (`SPOLE`), under "World and poles", for any model of the world list: `GfsRender::png` hands the chart's grids over (the same `fillOut` path the globe uses) to `WorldRender::renderWorld` on the polar projection and adds the screen's title lines, a color bar and the credit (`polarPng` in GfsRender.cpp). Drawn: GFS-ANL 10 mb 6 Jan 2013 (the split vortex) and the GFS 500 mb of the south pole. **The hover (the same day, the user: "the hover is one of the bedrock for the model viewer"):** the picture's probe (`GfsChart::Probe`) has a polar mode (`polar`, `polarUnits`: `locate` and `place` go through `GlobeMapView::inverse` / `forward`); its fields come from a small bare picture of the pole's box (`NPOLE-BOX`, `SPOLE-BOX`: 30 to 89.9 degrees) so the read-out has the fill, the height lines, the wind and its barb as on the flat charts; checked at the south pole (wind, height, barb), at the north pole 89 N, and at the New Jersey coast (39.5 N 74.8 W where the picture shows it); `WXQT_HOVER=<fx>,<fy>` on `chartpng:` prints the read-out. Not carried over yet: the overlays of other models, the marks of a place (the probe's `place` is there), the loop window not tried; the polar frame (`GlobeMapView::setPolarFrame(30, false)`) is the one global of the export, set at each polar picture.

**Open questions:** the start date to default to (1 October?) and whether the history is of the GFS only or compared with ERA5 for the same dates (the reanalysis is the better truth, a few days behind); the levels (10 and 50 mb first, then the rest); whether the history player is its own screen or a mode of the Model Viewer's loop.


## 20. Servers in one place: overrides, a server table and a status page (built 2026-11-15)

Raised while thinking about portability: NOAA's RRFS moved from `para` to `prod`, the IFS 0.1 degree set replaced the 0.25 degree one on 2026-10-20, a bucket can be renamed. Today each of those is an edit and a release. This sketches how a change of server could be made by the person, quickly, with the program saying which server it is reading and when one fails.

**What the program already has** (read from `GfsModelFile.cpp`, `GfsModels.cpp`, `GfsData.cpp`, `docs/model-files.md`, 2026-11-15):
- A model is a model file (`url`, `file`, `hours`, `lag_hours`, `probe_file` ...); the 19 built-in ones are text in `GfsBuiltInModels.cpp`, the person's own are `*.model` files in a folder (`GfsModels::setUserFolder`).
- **`url` is repeatable and the places are tried in order** (`GfsData::urlsFor`, `source.candidates`), so a fallback server is already one more `url` line. What is missing is memory: every request tries the first one again, a dead first server costs a failed request per file, and nothing says which one answered.
- `legacy_url = yyyyMMddHH => pattern` (one dated cutover, the IFS) and `alternate = ID | words` (a hint in the chart message: choose that other model; not a failover).
- The person's folder is read **after** the built-in models and **a file that repeats a built-in id is dropped** (`userProblems` says why). So a built-in model's server cannot be changed without a release.
- Settings has a "Model Sources" tab (`SettingsMain::showModelSources`) that checks the servers of the models; Help has "the sources check".
- Servers also live **in code**, outside the model files: `rrfsStream{"prod"}` in `UtilityGrib.cpp` (the older RRFS viewer path, NOMADS), the Open-Meteo and GEOS OPeNDAP bases, the SPC and WPC addresses of the point page. A cutover that touches them is an edit of code.

**The goal, as the maintainer put it (2026-11-15): a server URL change must not need a code change or a release.** Today every URL is in a built-in model file (text compiled into the program), so a moved bucket is an edit, a build and a release for everyone. The rest of this section is read against that: a fix is one edit of one data file, with nothing compiled, and it reaches the people who run the program (a) at once for the maintainer and (b) within a day for everyone else. Three layers answer it, from the person's own machine out:
1. **The person's own override file** (A below): a text file in their folder. No release, no network; fixes one machine, now. It is what a person does when the maintainer is not yet aware of a move.
2. **A published server file the program fetches** (F, promoted from optional): the maintainer edits one file in the repository (`servers.json`: the named servers of B, their bases and fallbacks, and the dated cutovers of E), and every install reads it at start and once a day. This is the layer that answers the goal: "RRFS moved" becomes one commit of one line. It overrides the built-in table and is overridden by the person's own file and Settings.
3. **The built-in table** (B): the program's own values, used when the fetch has not happened or fails, so the program works offline and on the first start.
What the published file may contain is deliberately small and checked, because it redirects where the program sends requests: **only the server table, dated cutovers and the lag / cycle numbers of a model, never code, charts, or a model's fields**; every URL `https` and on a host the program lists as a known data host (a short built-in list of the domains it already uses: `amazonaws.com`, `nomads.ncep.noaa.gov`, `googleapis.com`, `nasa.gov`, `open-meteo.com` ...) unless the person turns on "allow other hosts"; the file **not signed** (decided 2026-11-15: it is fetched over https from the project's own repository, so only the maintainer can change it, and the host list above limits what a bad one could do); the last good copy kept in the cache folder and used when the fetch fails or the file does not validate, with the reason in the sources status. The fetch is one request a day (conditional: `If-None-Match`), the generic `wxqt` agent, nothing about the person, never on a metered-looking pause (a failure is silent and retried the next day), and a Settings switch ("Read server updates from the project", **on by default**, since the point is that a person need not do anything; it is the one request the program makes to the project, said so in About and Help, and off means only layers 1 and 3).
**Where it is hosted:** a file in the repository (`servers/servers.json` with `servers.json.sig`), read from its raw address; or the project's GitHub Pages / a release asset if the raw address's caching (a few minutes) or rate limits get in the way. Not a server of its own: nothing to run or to keep up.
**Not covered, and said:** a change of *file format or layout* (a new variable name, a new grid, a changed index), which still needs a model-file edit; layer 1 covers that for one machine and layer 2 could carry whole model-file patches later, but that widens what the published file can do and is not proposed here.

**The pieces, smallest first**

**A. Override files.** A file in the person's folder with `override = GFS` (an existing id) changes only the keys it lists and leaves the rest of the built-in definition:
- *Replace* keys (`lag_hours`, `cycle_hours`, `hours`, `kind` is not allowed): the new value wins.
- *Repeatable* keys (`url`, `file`, `nc_variable`): the file's lines replace the built-in ones as a set, unless it says `url_prepend` / `url_append`, which put its places before / after (the usual case: "try this server first"). A `when` rule with the same condition replaces that rule only.
- Not overridable: `id`, `label`'s credit text (a credit follows the data's real source), `clone` rules, anything that changes which charts exist. The merged text is parsed as one definition by the same parser, so every error the parser has is reported by the existing `userProblems` with the file and line, and a file that does not parse leaves the built-in model untouched.
- Order of precedence: **Settings > the person's override file > built in.** The registry (`GfsModels::all`) merges once at the start, as it makes its list now; a change takes a restart (the registry is built once), or `GfsModels::reload()` when there is one.
- A tag on the model: "overridden by <file>" in the model's page and the sources check, so a person who forgets it can see why a model reads where it does.

**B. A server table.** A small list of named servers (`server = rrfs-ops | https://noaa-rrfs-ops-pds.s3.amazonaws.com | https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com`: a name, a base, then the fallbacks), built in as one more text in the built-in list and extendable in the person's folder. Model `url` lines use `{rrfs-ops}/rrfs.{date}/...` for the base. Then:
- A flip is **one entry**, and the models that share the server follow (REFS, RRFS and the RRFS ensemble share one bucket today; GEFS and GFS another).
- The code's own copies (`rrfsStream`, the OPeNDAP bases, the point page's SPC and WPC addresses) become table lookups (`Servers::base("rrfs-ops")`), so a cutover is never a code edit again. The built-in default stays in the program: the table never has to exist for the program to work.
- A person's change is a Settings entry (`SERVER_<name>` in the preferences: the base to use, optionally a fallback), which beats the file's, which beats the built-in.

**C. The status of each server** (what the Settings tab shows and what notices the person). For each (server, model) it keeps: the last time a request succeeded, the last time one failed and why (HTTP status, timeout, the file is not there), the newest run seen, the time it took, and the number of failures in a row. Kept in a small file in the cache folder (`servers-status.json`), not in the preferences. States, from those (the field states are in G):
- **OK**: the newest expected run (the model's cycle, less `lag_hours`) is there.
- **Late**: the run is later than `lag_hours` says it should be (a delay; do nothing, say it).
- **Down**: three failures in a row over at least ten minutes, or a 404 for the run's probe file for every cycle tried (`cycles_to_try`).
- **Retired**: Down for more than seven days. It is what the para-to-prod move would have looked like: the old server's runs stop and the new one's start.
- **Moved** (found by the sources check, not by a chart): the fallback server has a newer run than the first. This is the cutover signal; the first server may be about to go.
- **Sticky promotion:** the order of `url` places is a preference, not a fact. When the first place fails and the second answers for the newest run twice in a row, the second is tried first from then on (kept in the status file, with a notice "RRFS is read from its fallback server"), and the first is tried again every few hours (a failure costs one request, a recovery takes one success). A person's explicit order (Settings or an override) is never reordered.

**D. The Settings page.** The "Model Sources" tab becomes two parts:
- *Servers:* a table, one row for each named server: name, models that use it, the base now in use (editable), fallbacks, state, last OK, newest run, latency; buttons "Check now", "Use this fallback", "Reset to built in", "Copy as override file". A row's state is coloured the way the rest of the program colours status.
- *Overrides:* the files in the person's folder, each with the model it changes, the keys, and its problem if it has one; "Open folder", "New override for ..." (writes a starting file with the built-in `url` lines in it, to edit).
- In the Model Viewer, one line in the run menu's tooltip says which server the chart was read from, and a notice (the existing `notice()` line) appears when it is a fallback or a server is Late or Down: "RRFS: first server is down since 14:20, reading the fallback".

**E. Dated cutovers** (the general `legacy_url`): `url_from = yyyyMMddHH => pattern` repeatable, each a run time from which a place applies, so a model that changes its files twice, or a server that is switched at a known hour, is a few lines. The IFS's `legacy_url` becomes one of them (the old key stays, read as the first).

**F. The published server file (promoted by the goal above: on by default).** A short JSON file of server entries fetched from a fixed address in the project's repository, so a cutover is fixed for everyone the day the maintainer commits it. It is the only piece that makes the program contact the project, so, as in the goal above: a Settings switch (on by default, said in About), one request a day at most, the generic `wxqt` agent, nothing about the person in it, read-only (it can only set server-table entries, never code or a model's charts), shown in the Servers table as "from the project", and overridable by the person's own entry. Without it the other pieces still make a cutover a ten minute change for anyone who can edit a text file.

**G. Alerts for a change of format (the maintainer, 2026-11-15: a renamed variable is as much a break as a moved server, and wants at least an alert).** A model's files can change under the program without the address changing: a variable renamed (`TMP` to `T`), a level dropped, a file split in two, a unit changed, a grid reshaped, an index that now lists a record twice. Today the program finds out only when a chart fails with "does not have ..." and a person reports it. What it already has to build on (read from `GfsData.cpp`): the sources check (`GfsData::check`, behind Settings > Model Sources) reads each model's index (`.idx`, the ECMWF JSON index, the NetCDF header) for the newest run, but for a NetCDF model it says only "files found (variables not checked)", and for the others it checks the records of the chart it was asked about; `GfsData::load` fails with "<model> does not have <variable> for <run>". The alert is **a check of every field the model's charts ask for against what the newest file really lists**, run by the same daily check as the servers' status, and a record of what each model had last time:
- **The expected set:** for each model, the distinct (variable, level, detail) of every want of every one of its charts at the probe hour (`GfsChart::needs` over `GfsChart::products()` for that source), the file each is read from (`fileFor`), and, for a NetCDF model, the variable name the `nc_variable` rules give it. For a clone model that is a few hundred records; the check reads one index or header per file kind, not a field.
- **The listed set:** the records of the newest run's index (GRIB, ECMWF JSON: already parsed by `GfsGrid::parseIdx` / `parseEcmwfIndex`); the variables of a NetCDF file (GDAL's subdataset names; a `lev` dimension's values for the `@lev` files); Open-Meteo's `meta.json` variables for the `.om` models. The `custom` models (SPC REFS) answer by their own reader or are skipped, said so.
- **What it reports, per field and per model, in two kinds (the maintainer's words: "gone or not cataloged"):**
  - **Gone:** a want some chart asks for that the newest file does not list. Three grades: *Gone* (found at the last check, missing now: the strong signal, a rename or a removal, with the closest listed name for each: `TMP@10 mb` gone, `T@10 mb` listed, or the same variable at the nearest levels), *Never found* (asked for and never listed on this model: a chart that has never worked there, a smaller note), and *Partly gone* (listed for some hours or files of the run and not others: a file split or a run in progress).
  - **Not cataloged:** a record the file lists that no chart of the model asks for. The usual sign of an addition (a new level, a new variable: GEOS-FP's `EPV` was this until it was wired in), so it is **information, not an alarm**, and it is also a to-do list: the Details of a model has a "Not cataloged" tab (variable, level, unit if the index gives one, first seen), grouped (a new level of a known variable, a new variable), most recent first, with a "Copy as model-file lines" for a clone's `clone_fields` / `nc_variable` rules; the same list comes out of the project's daily check as a short file, so the maintainer sees what each model now offers that the app does not draw. A record listed twice (the ECMWF index's duplicates) is counted once.
  - The server state that was called *Gone* above (a server that is down for more than a week) is **renamed Retired**, so that Gone means only a field. A model's state is **Changed** when it has any *Gone* field (shown with the count: "4 of 212 fields gone"); *Not cataloged* alone never changes the state (a small "n new" figure beside the Fields count).
- **Where the alert shows:** (1) the Servers table's State and a "Fields" column ("212 of 212", "208 of 212"); (2) the Model Viewer's notice line for that model ("GEOS-FP: 4 fields are not in its newest file (T@10, U@10 ...): the charts that use them will not draw"), with "Details..." opening the list and the suggested names; (3) in the Help page of the model, the date of the last check and its result; (4) **"Copy report"** in the Details (the model, the run, the file kinds, the missing and listed names: nothing about the person) for an issue. Nothing is sent by the program on its own.
- **The maintainer's alert, without any report from users:** a scheduled workflow (a GitHub Actions cron, once a day on a Linux runner) runs the same check headless (`WXQT_OPEN=sourcescheck:*`, a route that writes `servers-status.json` and exits non-zero when a model is Down, Retired or Changed against the file kept in the repository) and, on a change, opens or updates one issue ("RRFS: 4 fields changed") and fails the run, which is the notification. The result file is also published beside `servers.json` (by the same https fetch), so an install can say "the project's last check (6 h ago): 4 fields of GEOS-FP changed": **the project checks, the installs read; no install reports anything to anyone.** A person's own machine runs the same check for the models they use, so a change that has not reached the project's runner yet (a regional file, a dev server) still alerts them.
- **A repair, where it is a name:** when the listed name for a want is unambiguous (one record, the same level and unit), the Details offer "Write an override that maps `TMP` to `T` for this model" (a `nc_variable` or file-rule line in the person's override file, A), so the person's machine works until the maintainer's fix reaches the published file or a release. The published file may carry the same mapping for everyone only if its schema is widened from servers to variable maps (the open point under F's "not covered").
- **Cost and manners:** one index or header per file kind per model per day (a few hundred requests across 70 models, range reads of kilobytes; the NetCDF headers by `Hdf5Range`), spaced, the generic agent, skipped for a model the person does not use (the program's own check), all of them on the runner's.
- **Tests, none needing a network:** a made-up index with one variable renamed gives Gone (with the right suggestion) for the old name and Not cataloged for the new one, and the model Changed; the same index with a record added gives Not cataloged and no alert; a want never listed gives Never found (the smaller note); the 'last time' record round-trips through `servers-status.json`; the expected set of a built-in model equals the set of the records its golden charts ask for (`tests/gfs/run_catalogue.sh`'s dump already lists the records and levels of each chart).
- **Limits, said:** it sees names, levels and the existence of records; it does not see a changed *unit*, a changed *meaning* under the same name, or a changed grid under the same name (a GEOS field of another grid reads as the first grid): a golden value check (a field's range at one known point against a stored range) could flag a unit change, and is listed as a later step, not proposed here. A file that lists a record twice (the ECMWF index's duplicates) is a New note, not an alert.

**Built (2026-11-15), step 1 and the first part of step 7, no signature (dropped by decision, 2026-11-15):** `src/gfs/Servers.*` (the table in four layers: built in, the project's `servers/servers.json` read once a day, your own `servers.json`, Settings; `{@name}` in a model's `url` gives one place for each base, the first first, and the model's candidates follow; `Servers::parse` refuses a file as a whole for a non-https base, a host the program does not read from (unless allowed), a bad name, a query, more than four bases or 64 KB), `servers/servers.json` and `servers/make_builtin.py` (the built-in copy), the Servers table, the daily-fetch switch and "Fetch now" in Settings > Model Sources, `docs/server-file.md`; the RRFS, GFS, IFS (and its legacy url) and GEOS-FP urls converted (the model-file golden shows the same first url for every want, plus the fallback candidates); `tests/gfs/run_servers.sh`. **Not yet:** the status record and the Servers table's state column, the override files of A, the other models' urls and the addresses in code (`URL.cpp` ECMWF and RRFS, `HurricaneData.cpp`, `UtilityGrib.cpp`), and the file is not pushed to the repository yet, so a fetch gets a 404 and the built-in table stands.

**First run of the catalogue check over every model (2026-11-15, a first check: nothing to compare with):** 75 models, 38 checked (the rest: Open-Meteo, Zarr stores, tar files, files with no index, local runs, no run on the server, no charts). **No field is Gone** (nothing to be gone from yet); **Never found:** GEFS 6 (the 30 mb charts the GEFS clone had listed: the GEFS files have no 30 mb, **fixed: `clone_levels` of the GEFS no longer lists it, four charts fewer**; and `RH@300 mb`, `VVEL@700 mb`), HRRR and HRRR-AK 13 each (`MAXUVV`, `MAXDVV@100-1000 mb`, `MINRH@2 m`, and the winds at 30, 50, 100, 160 and 320 m: charts that cannot draw there), NBM 2 (`TMAX`, `TMIN` at 2 m), RRFS 1 (`MINRH`), GFS-ANL and GFS1-ANL 1 (`DPT@2 m`); 2 models had files it could not read (RRFS 1, REFS 5). **Not cataloged:** the most are CFS-ANL (7,178: an index of many times), GEFS (1,947), REFS (1,170), RRFS (896), the regional RRFS (880 each), GFS 641, HRRR 757. Not yet: a first-class report page, the gone grade (it needs a second check), the check run daily by a workflow.

**Status record built (2026-11-15):** `src/gfs/ServerStatus.*`: every Settings > Model Sources check of a model (`GfsRender::checkSource`; not a skipped model, not a local problem such as a missing GDAL driver) is kept in `servers-status.json` (the last check, the last OK, the newest run seen, the run of failures and when it began, the model's cycle and lag); `stateOf` gives **OK** (the newest run is not older than cycle + lag + 3 hours; an archive is never late), **Late**, **Failing** (the last check failed), **Down** (two or more failures at least ten minutes apart), **Retired** (down, last OK over seven days ago), **Unknown**; the Model Sources table has **State** and **Last OK** columns, shown from what was kept before the check is run; the Help page says what the states mean; `tests/gfs/run_serverstatus.sh` (made-up times). Seen: 71 of 75 models OK, 4 with problems. **Then built (2026-11-15):** (1) **Moved** and the sticky fallback: when no run is found, `GfsData::latestRun` asks the probe file at the model's server's other bases; one that answers (at another host) goes first for the rest of the run (`Servers::promote`, in `table()`/`places()`; `reload()` starts over), and the state is Moved with the answering host (`ServerStatus::setPlaces`; `GfsData::placesOf`); tested live with a dead first base. (2) **State column in Servers by name**: the worst state of the models that read from it (`ServerStatus::Entry::server`), the tooltip lists them. (3) **Changed**: `CatalogCheck::keep` records the gone fields (`ServerStatus::setChanged`). Order: Retired, Down, Failing, Moved, Changed, Late, OK. (4) **Headless**: `WXQT_OPEN=sourcecheck:<model|*>:<out.json>`; the daily workflow runs it before the catalogue check, keeps `servers-status.json` in the cache beside `catalog-status.json`, and opens one issue (label `source-status`) when a source is Moved, Down or Retired. **Fallback kept (2026-11-15):** a promoted base is saved in `promoted.json` and used for a day across restarts; after a day the first place is tried again (and promoted again if still dead). **Signing dropped:** the published file is fetched over https from the project's own repository and checked on arrival (https, known hosts, size, all-or-nothing), so a signature adds a key to guard for little: revisit only if others publish or mirror server lists or the file may name any host. No signed published result file either.

**Steps, each useful alone** (reordered by the goal: 1, 2 and 7 give a fix with no release; 3 to 5 make it visible)
1. **The server table and `{name}` tokens** in the model files, the built-in table, `Servers::base`; the RRFS, REFS and GEFS URLs converted; a test that every built-in model expands to the same URLs as before (as `run_modelfile.sh` does for the files). No behaviour change.
2. **Override files** (A) with the merge rules and their tests (replace, set, prepend, append, a bad file leaves the model alone, the id check).
3. **The status record** (C) written by the code that already makes the requests and the run probe (`GfsData` run search, `probe_file`), the state machine with a fake clock and a fake fetch (no network): OK, Late, Down, Retired, Moved, Changed, sticky promotion and the retry.
4. **Settings: the Servers table and the overrides list** (D), the notice line in the Model Viewer.
5. **Dated cutovers** (E).
6. **Code constants** (`rrfsStream`, the OPeNDAP bases, the point page) moved to the table.
7. **The published file and its reader (F)**: the format, the signature check, the cache of the last good copy, the daily conditional fetch, the host list; then the first real use: the RRFS, REFS and GFS bases in `servers.json`. (Move it to step 3 if the aim is to have it before the next cutover.)

**Tests, none needing a network:** the expansion of every built-in model before and after step 1; the merge of an override (keys replaced, repeatable keys, a rule replaced by its condition, an error reported with its line, the built-in left whole on a bad file); the status states from a script of successes and failures at fake times; the sticky promotion and its retry; a settings entry beating a file beating the built-in.

**Open points:** (0) the fifth state's threshold: Changed on any want gone since the last check, or only when a share of a model's wants (and one chart's whole set) is gone; (0b) whether the project's published check result should also gate the published `servers.json` (a server entry is not applied when the project's own check says the model is Changed there: no); (1) whether the registry can be reloaded without a restart (it is made once, `static`; the Model Viewer holds `Def` pointers); (2) whether a changed server should change the cache key of what was read from it (the same run on another server is the same data, so no; a different product under the same name, para against prod, is a different run and the run id already differs by date, but a model that keeps its date and changes its numbers is not covered: an override can add `cache_tag`); (3) how much of the status is shown without being asked (a banner for Down and Gone only, the rest in the Settings table); (4) whether the sources check should run on its own once a day and write the status (a handful of HEAD requests for each server) or only when asked.

## 21. The product catalogue as data you can read and edit (chart files, the Catalogue page and "Make a chart" built 2026-11-15)

The maintainer: the catalogue should be **editable and accessible**, not only code. Today the catalogue is C++ (`GfsProducts*.cpp`, 19 families of functions, about 5,580 chart entries after the clones), with a read-only dump (`docs/presets/charts-dump.txt`, a test compares it) and the alias table (§9, generated). Section 11 already named the wish (a data file over definitions in code). The catalogue check built on 2026-11-15 (`CatalogCheck`, `GfsData::inventory`, `WXQT_OPEN=catalogcheck:`) is the first thing that reads the catalogue as data (the wants of every chart), and its "Not cataloged" list is the natural place to start adding to it.

**What can be data and what cannot.** A chart is a fill, lines, barbs and a title over **wants** (a variable at a level in a file) with a scale; most are one of a few shapes (a field, a speed of two fields, a difference, a sum, a unit change). The code in a chart is lambdas for the rest (a derived grid: PV, thetae, the hail index; the REFS statistics). So the catalogue splits in two, and only the first moves out of code:
- **Recipes in data** (the easy majority): `fill` = a field or one of `speed(u, v)`, `a - b`, `a * k + c`, `mean(a, b)`; `contours` (a field, a scale, an interval, a color, `highs_lows`); `barbs` (u, v); `ramp` = the name of one of the program's 45 scales (and optionally the units for it); `label`, `group`, `quantity`; the wants are implied by the fields named, with the model's own variable names mapped by the model file. This covers the stratosphere charts, the GEOS-FP EPV chart, a new level of a known variable, most of what the "Not cataloged" list will offer.
- **Recipes that stay code** (named, not editable): anything with a `derive` that is not one of the above. They show in the catalogue as "built in (code)" with their wants listed, so the check and the browser still see them.

**Accessible: one read-only file, always there.** `catalog.json` (a command `WXQT_OPEN=catalogexport:<out>` and a button in Settings, "Save the chart catalogue..."): every chart of every model with its id, label, group, wants, shape (recipe or code), scale name and the model it is cloned from. The golden `charts-dump.txt` becomes this file (so the test and the export are one thing). A "Charts catalogue" page in Settings lists it with a search, by model, with what each chart reads and whether the model's newest file has it (the check's result beside it).

**Editable: chart files, like model files.** `*.chart` (or JSON) in the person's models folder: a recipe for one chart on one model (or on a family: "for every model with these wants"), the same checks as a model file (a plain name, known fields, a known scale, a url-free text), reported by `userProblems`. It appears in the Model Viewer under its model in a group "Your charts", in the Checks picker as a one-off, and its wants count as cataloged for the check. The "Not cataloged" Details get "Make a chart from this field..." that writes the starting file (the field as the fill, a scale picked by its unit, a label from the name) for the person to edit.

**Published additions (the servers.json route, later):** the same recipes in a published, validated file the program reads daily (`servers/catalog-additions.json`), so a new field or level is **a chart the maintainer adds with no release**; the same limits as the servers file (data only: no code, no new hosts, a recipe may only name fields and scales the program knows), and the person's own files over it. The check's "Not cataloged" list is then a to-do list that turns into a commit of a recipe.

**Built, steps 1 and 3 in a first form (2026-11-15):** `src/gfs/UserCharts.*`: a `*.chart.json` in the models folder is a chart of one model (`id`, `model`, `label`, `group`, a `fill` of a field or the speed / difference / mean of two with a scale and offset and one of 29 named ramps and a `quantity`, `contours`, `barbs`); it is read after every built-in chart when the registry is made (`GfsChart::products()`), appears in the Model Viewer under the model in "Your charts", and its problems join `GfsModels::userProblems()`; a file that is not valid is left out as a whole; an id a model already has is left out. `docs/chart-files.md`, the example `docs/examples/newsurface1.chart.json` (2 m temperature as the area, 500 mb heights as lines, 10 m barbs: drawn through `chartpng:GFS:newsurface1`), `tests/gfs/run_userchart.sh`. **Not yet:** step 2 (`catalog.json` and the Settings page), step 4 ("Make a chart from this field"), 5 and 6, a derived field (PV and the like stay code), the catalogue check counting these wants as cataloged (it does: they are in `GfsChart::products()`), a reload without a restart, and a Settings list of the files with their problems.

**Steps, each useful alone:** (1) the recipe format and its reader (a pure parse into a `Product`: fill, contours, barbs, ramp by name), with tests that rebuild three built-in charts (a speed chart, a temperature chart with lines, a PV-like difference) from recipes and draw them to the same golden pictures as the code's; (2) `catalog.json` export and the Settings page (read only); (3) `*.chart` files from the models folder into the registry, "Your charts" in the picker, `userProblems`; (4) "Make a chart from this field..." from the check's Details; (5) move the simple built-in families into recipes (the stratosphere set first) so the data is the catalogue and the code only the named derivations; (6) the published additions file.
**Open points:** whether a recipe may name another chart's pieces ("500 mb heights of this chart"), the unit story (a chart in data needs the quantity so °C/°F conversions work: `quantity`), which fields can carry a `detail` (the blend's percentiles, the REFS members), and whether the Checks picker's alias table (§9, generated from the dump) is generated from the same file.

## 22. Ensemble percentiles at a place (2026-11-15)

Asked: the ensemble's statistical results for a place, percentiles of temperature and precipitation as a chart. **Built:** `src/cfs/EnsemblePercentiles.*` (no window, no network of its own in the numbers: the percentile rule, the fan of percentiles in time, each member's running total, the share of members with at least a threshold in each 6 hour block, the parse of the service's answer; `tests/gfs/run_ensemblepct.sh`) and `src/cfs/EnsemblePercentileViewer.*` (the window: a saved location or a latitude and longitude, a model, Load). The members come from Open-Meteo's ensemble service (`ensemble-api.open-meteo.com/v1/ensemble`: one request gives every member's hours for the place, so nothing is downloaded by the field): GEFS (31), ECMWF IFS (51) and AIFS (51), the Canadian GEPS (21), DWD ICON (40), the UK Met Office (18). The chart has three panels: 2 m temperature (lowest to highest member, 10 to 90 and 25 to 75 percent bands, median, mean), the precipitation added up since the first hour (the same bands), and the chance of 0.01 in or more in each 6 hours (the share of members), with a read-out of the numbers at the cursor; units follow the Settings (deg F / in or deg C / mm). It is in the toolbar's list beside the forecast plumes; headless: `WXQT_OPEN=ensemblepct:<lat>:<lon>:<model id>:<out.png>`. **Not yet:** the click on the master map (the plume has one), a second precipitation view as period amounts (box per 6 hours), snow and wind gusts, the NBM's own percentiles (a probabilistic product, kept in the point forecast model rather than made from members), local time on the axis, the cache is 20 minutes in memory only, and the members' values are the service's own at its grid point (no elevation or bias correction).

**Sources, decided 2026-11-15 (the user: prefer others to Open-Meteo where there is an alternative; the text forecasts):** the first choice in the list is now the **NWS National Blend of Models probabilistic text bulletin (NBP)** of the nearest station (`NbmText::fetchProbabilistic`, read by byte ranges from the public AWS bucket `noaa-nbm-grib2-pds`, `blend.<date>/<cycle>/text/blend_nbptx.t<cycle>z`; the existing station lookup, bisecting reader and parser, with five character codes for NBP): every 12 hours from 24 to 264 hours, **10, 25, 50, 75, 90 percent** of the highest (00 UTC) and lowest (12 UTC) temperature (`TXNP1 P2 P5 P7 P9`, mean `TXNMN`, standard deviation `TXNSD`), the 24 hour precipitation ending at 12 UTC (`Q24P*`, hundredths of an inch), the 24 hour highest wind and gust ending at 00 UTC (`W24P*`, `G24P*`, knots), the wind speed and pressure at each time. Drawn as box-and-whisker (ECMWF's own layout for its meteograms: median, 25 to 75 box, 10 to 90 whisker). **Open-Meteo remains** only for what a bulletin cannot give: an hourly fan from the members (a different thing: members, not the blend's calibrated distribution), places outside the United States, and the other ensembles (IFS, AIFS, GEPS, ICON, UKMO). Not yet: the blend's gridded percentiles for any point (the `qmd` files hold every 5 percent level of temperature and others, but a record is 1.5 MB at each hour and level: about a gigabyte for a week at one place, so not by range reads), the NBS and NBE columns (the 3 and 12 hourly deterministic values and the probabilities of precipitation) over the boxes, ECMWF's open data members from `ecmwf-gcs` (the free IFS ensemble, 51 members: per member per hour a range read, like the GEFS plume), and the GEFS members from NOAA's own files (the plume already reads them).

**The Euro, global (2026-11-15, the user: "Euro global too"):** three choices in the list show **ECMWF's own ENS charts** for any latitude and longitude in the world, made by ECMWF's Open Charts service (`charts.ecmwf.int/opencharts-api/v1/products/opencharts_meteogram/?epsgram=classical_10d|classical_15d|classical_plume&lat=..&lon=..`: one request for the address of the picture, one for the picture; CC BY 4.0, the copyright and source are printed in the picture): the box-and-whisker meteogram of cloud, precipitation, wind and temperature (10 days: 6 hourly; 15 days: daily, with the wind rose) and the plumes. **It is a picture, not data**: the service gives no numbers, so these cannot be mixed with the blend's boxes or read at the cursor. **Why not the numbers:** the free ECMWF open data on `ecmwf-gcs` has the 51 members only as whole global fields (about 1 MB a field per member per hour: some two gigabytes for ten days of 2 m temperature at one place), and the ensemble mean and spread (`em`, `es`) would be a normal curve, wrong for rain. Not yet: the GEFS and IFS ensemble means and spreads at a place as a quick normal band, and a cropped picture (the chart has a blank band at the foot).

## 23. Model comparison at a place (2026-11-15)

Asked: "like for like trends of model bias" ("the Euro is cooler than the GEFS. Why? ... because it brings the jet stream further south"). **Built, first step (model against model):** `src/cfs/ModelCompareViewer.*` (the window, in the toolbar list; headless `WXQT_OPEN=modelcompare:<lat>:<lon>:<model A>:<model B>:<temperature|precipitation>:<out.png>[:why]`) and `EnsemblePercentiles::compare` (the two models at the hours they share: each one's median and 25 to 75 percent band, the difference A minus B of the medians and of the means, the daily means of the difference over whole UTC days, the mean of it over the first 3, 5, 10 days; tested). The chart: both models over each other, and below them the difference (the hours as a line, each day's mean as a bar, warm colour when A is higher). **Why?** (a click on the chart picks a time; with none picked, the day with the largest difference): the Model Viewer opens on that valid time (the six hours at or before it) on the first model's 500 mb wind and height chart (or the 250 mb jet) with the other model's height lines over it (`overlayModels`: the program's existing "Overlay models..."), falling back to the other model's chart when the first has none; the IFS ensemble has no height chart, so the ECMWF high resolution run (`IFS`) stands for it, and the status line says so. Seen with the IFS ensemble minus the GEFS at 39.1, -94.6: the Euro 2 to 3 degrees F cooler on average for the first 5 days and 5 to 10 degrees cooler at 24 to 25 Nov, and at 24 Nov 12Z the IFS height lines (red) have the trough much further south over the central United States than the GEFS (black): the example the user gave, drawn. Members by Open-Meteo (no alternative gives members at a place); the maps are the program's own. `chartpng:` gains `WXQT_VERSUS=<model,model>` (the overlay models). **Not yet:** the second step, **bias against what happened** (the models' past forecasts against the nearest station's observations by lead time; needs the runs kept or a past-runs service), the **run-to-run strip** (is the gap steady, growing or flipping over the last runs: the program would keep each load's medians by model, place and run), a one-line sentence of the likely reason (the height difference over the area, in numbers), the area of the map following the place (it opens on the screen's last area), and a place's local time on the axis.

**Spaghetti view (2026-11-15, the user: "in the same idea as spaghetti plots"):** the comparison window has a "Spread" choice: the 25 to 75 percent bands, **every member as a thin line** (each model in its own colour, the medians bold over them), or both (`EnsemblePercentiles::onTimes` puts each model's members on the common hours; `WXQT_SPREAD=1|2` headless). The members' lines show at once whether the gap between the models is the whole ensemble moving (two clouds that do not overlap) or only the medians. **Not yet:** the same lines in the single-model percentile window, the spaghetti on the Why? map (the program's spaghetti charts, e.g. the 552 dam contour of every member, exist for the GEFS and the IFS ensemble under different chart ids: the two cannot be overlaid as they are), and the difference panel drawn as one line per member pair.

**Box and whisker view (2026-11-15, the user: "perhaps a box and whisker comparing the model distributions"):** a fourth "Spread" choice draws the two models side by side every 12 hours (00 and 12 UTC): whiskers the 10th and 90th percentiles, box the 25th to 75th, a bar for the median (`Compare` gains `aP10 aP90 bP10 bP90`; `WXQT_SPREAD=3` headless). Not yet: the lowest and highest member as the whisker ends (an option), more than two models in one row, daily highs and lows as the boxes' times.

**Every ensemble at one time (2026-11-15):** "All models at this time" (the time picked, else the day with the largest difference) opens a window with a box for each ensemble of the list side by side, sorted by median: whiskers lowest to highest member, a thick whisker 10 to 90 percent, the box 25 to 75, a bar for the median, and every member as a dot beside it (`WXQT_ALL=1` headless). Seen at 39.1, -94.6 for 24 Nov 12 UTC: the GEPS coolest (median 32 F), the IFS and AIFS near 37, the UKMO 38, the GEFS warmest (49 F): the GEFS is the outlier, not the Euro; the ICON (7 days) has no value then and the status line says so. Not yet: the same at several times in one picture, and a model's chosen run (all are the newest).

**The easy adds (2026-11-15):** (1) **Variables as a table** (`EnsemblePercentiles::variables()`: id, label, title, quantity, whether it adds up, whether the axis starts at zero, digits; `display` and `unitLabel` turn a value into the person's units; `fetchOne` reads one variable of one model, kept 20 minutes): the comparison and all-models windows now offer 2 m temperature, precipitation added up, 2 m dewpoint, 10 m wind speed and gusts (not every model has the gusts; a model without them is left out and named), snowfall added up, sea level pressure, cloud cover and 2 m relative humidity; a new variable is one row. (2) **More ensembles** are rows of `models()` (and `mapModel` for the map): the AIGEFS (the AI ensemble, 31 members) and the ICON-EU (Europe only: elsewhere the service says it has no data and the window says so). The service's BoM ensemble returned no values for the first days and was left out. (3) The service asks for wind in m/s (`wind_speed_unit=ms`) and the table turns it into mph or keeps it by the Settings. **Not yet:** the percentile window's three panels are still temperature, rain added up and the chance of rain (they are the structure of that window, not a choice), an editable file for the two tables (as servers.json and the chart files are), local time on the axes.

**Local time (2026-11-15):** the members' windows (the comparison, all models and the percentile window's member mode) now use the place's own clock: the request asks `timezone=auto` and keeps `utc_offset_seconds` (`Series`, `Fan`, `Compare::utcOffset`), the day lines and day names are at the place's midnight, the 12 hourly boxes at its noon and midnight, the daily means of the difference over its whole days, and the read-outs say "local". The NBM bulletin view and the maps of "Why?" stay in UTC (a bulletin's columns are UTC; the model valid times are); the 6 hour rain bars of the percentile window are UTC blocks and the caption says so. The offset is the one the service gives for the place now (a daylight saving change inside the 16 days is not followed).

**The tables are a file (2026-11-15):** `ensembles.json` in the folder of the person's own models (the one `servers.json` is in), read over the built-in tables of models and variables (`EnsemblePercentiles::models()`, `variables()`, `parseFile`, `reload`, `problem`): a new key adds a row, an existing key replaces it; a model row has `label` and `map` (the program's own model for "Why?"), a variable row `label`, `title`, `quantity` (temperature, precipitation, wind, snow, pressure, percent, or `plain` with its own `unit`), `accumulate`, `zero_based`, `digits`; the file is refused as a whole (reason in the window's status line and tooltips) for a bad name, a missing label, an unknown quantity, a plain variable with no unit, digits out of range, more than 60 rows or 64 KB; `docs/ensemble-file.md` is the Help guide (Help > Editing the program's data, and a Help... button in both windows), `editing-rules.md` has its row; the built-in `mapModel` table is now the `map` of each model row. Seen: a CAPE variable added by the file draws in the comparison with its own unit; a bad file is refused with its reason. Tested: valid entries, each refusal, plain units. **Not yet:** "Your file..." button that opens the folder from these windows (Settings > Model Sources has one for the same folder), a template written for the person on first use, a catalogue of the service's variable names to choose from.

**Bias against what happened (2026-11-15, the second step of section 23):** `src/cfs/ModelBias.*` (the statistics, the parse and the requests; `tests/gfs/run_modelbias.sh`) and `src/cfs/ModelBiasViewer.*` (the window, in the toolbar's list; headless `WXQT_OPEN=modelbias:<lat>:<lon>:<model A>:<model B>:<variable>:<days>:<lead>:<out.png>`). The past forecasts are Open-Meteo's **previous runs** service (`<variable>_previous_dayN`, 1 to 7 days ahead, for the deterministic models: the IFS, GFS, AIFS, GDPS, ICON, UK Met Office, ARPEGE, JMA; the leads a model does not keep are blank), the truth is **ERA5** from the archive service (a reanalysis, not station observations; it ends about five days before the present, so the newest days are left out). The chart: for each lead the **bias** (forecast minus truth, the bars; positive is too high) and the size of the miss (the ticks), and below the **error of each day** at the lead chosen with the mean of the last five days as a thick line (the drift). An added-up variable (rain, snow) is judged on the day's total. Variables are those of the ensemble windows. Seen at 39.1, -94.6 over 30 days, 3 days ahead: the Euro 0.3 F too high (misses by 1.9), the GFS 2.3 F too high (3.0), the GFS error growing through November. **Not yet:** station observations as the truth (the nearest station's daily highs and lows from ACIS, which the program already reads for its history screens, would judge the highs and lows the way a forecaster does and has no five day lag), the ensemble means in place of the deterministic runs, a "last 7 days" without the ERA5 lag (the models' own analyses as the truth), bias by time of day, and the link from this window to "Why?".

**Judged on a station (2026-11-15):** the bias window's "Judged on" choice now has the nearest weather station's **daily highs, lows or rain** as the truth (`ModelBias::fetchStation`, `parseStationList`, `parseStationDaily`, `evaluateDaily`; NOAA's Regional Climate Centers' ACIS: `StnMeta` for the stations in a box of 0.9 degrees, `StnData` for the days; the nearest of the eight nearest that has 60 percent of the days is used, since a station on the list may have none; the status line names it and its distance). The forecast is reduced to the place's own days (the highest, the lowest, or the total of the hours) and judged against the station's day of the same date; temperatures are kept in deg C and rain in mm inside (ACIS is in F and inches), a trace is zero, a missing day is left out. No five day lag. Seen: Kansas City, highs, 3 days ahead, 30 days: the Euro 0.3 F too high (misses by 2.7), the GFS 2.2 F too high (2.9), station KANSAS CITY INTL AP, 25 km: the same story as against ERA5. The observers' day is usually midnight to midnight in local standard time, so in summer an hour of the day is shifted: said in the status line. **Not yet:** a station that the person picks, the wind and the other variables (ACIS has them for first order stations only), the ensemble means instead of the deterministic runs.

**Forecast evolution (2026-11-15, the run-to-run strip of section 23, from the previous runs):** the bias window's lower chart can show **how the forecast for each day changed** instead of the daily error: for each day, each model's value as made 7 days ahead (pale) down to the day itself (dark) joined by a thin line (the first model left of the day's middle, the second right), and what happened as a black diamond (`ModelBias::dailyOf`: the day's mean, highest, lowest or total by what is judged; `WXQT_EVOLUTION=1` headless). It shows the thing the run-to-run question asks, without keeping anything: whether a model's forecast for a day converged early or flipped late, and whether the two models were apart a week ahead and agreed by the day. Seen at Kansas City, highs: both models' sticks are long a week ahead and short by the day, the diamonds inside the dark dots most days; the large misses (22 Oct, 29 Oct) are one model's pale dots far from the rest. **Not yet:** the ensemble members' evolution (the previous runs service has no past ensemble runs), the same for a coming day (the newest runs only have the leads 0 to 2 for days not yet seen), a click on a day to open its maps.

**Why? from the bias window (2026-11-15):** a click on a day of the lower chart picks it (shaded), and "Why? Show the maps" opens the Model Viewer on that day at local noon (the six hours at or before it) with the first model's 500 mb chart and the second model's height lines over it, as the comparison window does (the opening is now `ModelCompareViewer::showMaps`, used by both; the bias models carry the program's model for the map: `ModelBias::Model::map`, blank for the GDPS and JMA, which the program has no map of); with no day picked it opens the day where the two models' errors were furthest apart (Kansas City highs, 3 days ahead: 27 Oct). The sentence of the status line says "no clear bias" when the bias is under a quarter of the misses. **Not yet:** the maps keep the screen's last area (not the place's); a past day's maps need that run to still be on the server (the models differ in how far back they keep runs), and the Model Viewer says when it is not.


**Polish of sections 8 and 21 (2026-11-16).** *Section 8:* `tests/gfs/run_nbmtext.sh` (real KOKC blocks of NBS, NBE and NBX in `tests/gfs/nbm/`: the columns and their times, the highs and lows under the right columns, the CLIMO pair of the NBE not read as a column, the merge in order with each source tagged, the finder on a stand-in file of four sorted runs; and with `NBM_FILE_NBS|NBE|NBX=<the whole file>` the finder on **every station** of the real file: 9,589 of 9,589 in the NBS and the NBE, 9,352 of 9,352 in the NBX). It found a real bug at once: the last station of a file was never found, because the imaginary `~END~` header the reader gives the end of the file did not match the header pattern (`[A-Za-z0-9]+`); fixed. The extended tab now shows the person's units (the Settings: F and mph, or C and m/s; the bulletins' F and knots are converted when the table is built; the spread is a difference). The "which run did it use" line was already there ("NBS 09Z - NBE 09Z - NBX 09Z"). Not done, and left: precipitation type as colours (the bulletin's type codes are not read), the map thumbnail, NBH, whether the place's station is kept per saved place (it is looked up each time: a request for the nearest stations, cheap). *Section 21:* the Help buttons (see `help-coverage.md`); **chart files reload without a restart**: the registry of charts is made in generations (`GfsChart::refreshUserCharts`: the built-in charts and the folder's `*.chart.json`, made again when the folder's files, sizes or times changed; the old generation is kept, so a pointer a screen holds stays good), the Model Viewer asks when it opens, and Settings > Model Sources has "Reload your files" (servers.json, chart files, ensembles.json; the problems are listed in the status line); `tests/gfs/run_userchart.sh` writes, changes, removes and breaks a chart file in a folder and watches the registry follow. The `.model` files are still read at the start.

**The catalogue page and "Make a chart" (2026-11-16, section 21):** Settings > Catalogue (`SettingsCatalogueBox`): a model, Check (`GfsRender::checkCatalog`, the index only), a table of every field with its state (Gone, Never found, Found, Not cataloged) and the charts that use it, a filter, and **Make a chart** (`UserCharts::starter` / `writeStarter`, the C++ of `tools/make_chart.py`: the ramp, the units and the legend step from the variable; the file `<variable>_<level>.chart.json`, another name when it is taken; read at once through `GfsChart::refreshUserCharts`; `tests/gfs/run_userchart.sh` checks the starters are valid chart files); headless `WXQT_OPEN=cataloguepage:<model>:<filter>:<out.png>` (`WXQT_MAKE=1` makes a chart from the first row: seen on GFS, 90 of 90 fields found, 641 not cataloged, a chart made from CLMR@1 hybrid level and accepted by the parser). `docs/chart-files.md` says how. Not yet: the catalogue of a model with a state kept from the daily check (the page checks one model when asked and has no memory: the daily workflow's `catalog-status.json` is the memory, on the runner), a column for the model's other levels of the same variable, contours and barbs in a starter (only the fill: edit the file for lines).

**Section 20, finished (2026-11-16):** (1) **Every model's `url` is on a server name** (71 lines of `GfsBuiltInModels.cpp`; `servers/servers.json` and the built-in table have 23 servers now: the NOAA buckets, `nomads`, `ncei-models`, `dwd-opendata`, `msc-datamart`, `openmeteo-s3`, `openmeteo-seasonal`, `nssl-thredds`, `spc-refs`, `wpc-ppp`, `woof-nam` besides the four before; the model-file golden gained the `candidates=` of every model; checked live: GEFS, HRRR, NBM, AIGFS, ICON, GEPS, RTMA, MRMS, GFS-ANL, CFS, IFS-ENS and EC46 find their runs; HREF fails as before, it is retired); (2) **the addresses in code** read the table too (`Servers::first(name, fallback)`: the NBM bulletin bucket, the MRMS listings, the CFS bucket and its reanalysis, the EC46 bucket, NOMADS for the SPC post-processing, the older RRFS and REFS paths, the HAFS folders, ECMWF's open data in `HurricaneData` and `URL.cpp`'s mirrors), each with the old address as its fallback (not changed: the host tests in `NetManager` and `CfsArchiveIndex`, the Open-Meteo API addresses, the NWS and SPC services of the point pages, the GOES, radar and warning feeds: those are services, not model servers); (3) **override files** (`override = GFS` in a `.model` file: `GfsModelFile::mergeOverride`, applied by `fromFile` so the models the code builds on top of a file (RRFS, HRRR, the analyses) follow; `Def::overriddenBy`, "read with your file" in the sources check and the model's Help page; `tests/gfs/run_override.sh`: the merge, each refusal, and a built-in model read with the person's lines). **Not done:** the state of a *model* with its server in the Model Viewer's run menu tooltip and the notice when a fallback is in use (section C's last paragraph), "Copy as override file" and "New override for ..." buttons (the file is written by hand from the Help page), a model-file patch in the published file (said not proposed), and an override that takes effect without a restart (the registry is made once).

**Section 10, the master map's model layer (2026-11-16):** the layer now holds the cross fill (`ModelChartLayer::crossFill` / `fadeFill`, kept in `MM_MODELCHART_CROSS` and `MM_MODELCHART_CROSSFADE`, carried by `selection()` and `apply()`, so a chart handed from the Model Viewer arrives with its second field; `MasterMapViewer` writes the two prefs with the others), it is part of the layer's picture key (`|cross=model:chart` and `|fade`, so the cache keeps one picture for each choice), the layer's legend is left out while another model's field is over the chart (it would describe only the base fill), the model bar has the same **"Another model's field..."** button as the Model Viewer (the dialog is `ChartPickers::chooseCrossFill`; the first time the base fill is faded), and a change of model clears it; the request is `selection().variant(...)`, the same as the Model Viewer's, so the picture is drawn by the same code. `mcexport:` takes `PREF=value` parts: a `|` stands for a `:` in a value (`MM_MODELCHART_CROSS=SPCPOST|thunder_4hr`). **Not verified by a picture:** `mcexport` could not draw any chart in this environment (the layer had no run loaded within its 40 seconds, with or without a second field, and the map tiles failed with "Unknown protocol"), so the layer's drawing of the second field is checked only through what it shares with the Model Viewer (the golden `cross_fill`, `features_test`, the `chartpng:` pictures). **Not done:** the world export (`WorldRender`) and the history dialog do not draw the second field, a second field in the compare tiles, the "from" value of the cross fill (a sixth of the scale is cleared whatever the field).

**RTMA area bug (2026-11-16, the user: "the RTMA viewer is rendering the CONUS, but not zooming to CPlains"):** the NWS analysis window opened from the toolbar and from the home screen (`AnalysisLoop::openLatest`), and the two RTMA tiles of the home screen (`HomeThumbnails`, `RTMA_AREA` and `RTMA_SECOND`), took "the area last used" from the Model Viewer but drew any area that was not a saved place as the contiguous US, so the Central Plains (and every other NWS area) came out as CONUS. They now ask `GfsChart::areaFor("RTMA", last area)`: a saved place or one of the model's own areas stays (the RTMA has the 14 NWS areas of the blend's list), anything it cannot draw (Europe, the Atlantic) is CONUS. Checked: `chartpng:RTMA:2m_temp_10m_wnd:0:C-PLAINS` already drew the zoomed chart (the renderer was right; the windows asked it for CONUS); `tests/gfs/run_features.sh` checks the helper. The Model Viewer's own "NWS analysis loop" button passed its area already.

**The polar vortex on the home screen (2026-11-16, the user: "a constant 10mb/isentropic view of the North polar"):** two home thumbnails, off by default (`HomeThumbnails`: `VORTEX_10MB`, the GFS 10 mb wind and height, and `VORTEX_THETA`, the 850 K isentropic potential vorticity, wind and pressure, 850 K being near 10 mb), each the north pole to 30 N on the polar projection, painted from the grids themselves by `WorldRender` (the master map's export code: `loadSources` and `renderWorld`, no widget needed) for the newest initial state (six hours back, at a six hourly time, so the run has it), with the run, hour and time written above; a click opens the master map on the same chart over the pole (`MasterMapViewer::openModels(..., "POLAR")`). Headless: `WXQT_OPEN=thumb:VORTEX_10MB:<out.png>`. Seen: the 16 Nov 06Z vortex displaced toward Scandinavia in both. **Not yet:** a legend (the picture is the map only: the master map has it), the other models (the Euro's IFS has the 10 mb chart too: a choice of model in the tile's settings), the south pole, the history strip as a tile, the refresh schedule is the home screen's own (the picture is made when the tile is, not every six hours on its own).

## 24. The RRFS aerosols, and what else the files had (noted and built 2026-11-16)

The RRFS's `2dfld` files (`rrfs.t<HH>z.2dfld.3km.f<FFF>.conus.grib2`, every hour, also `ak`, `hi`, `pr` and the 13 km `na`) carry the smoke and dust model's fields. The app reads only one of them: `MASSDEN` at 8 m, "Total aerosol", for the nice weather outlook's smoke cap (`GfsProductsNbm.cpp`). None of these is a chart, and none is used anywhere else (checked in the 06Z run of 16 Nov 2026, f012):

- **`AEMFLX`, entire atmosphere: the emission flux**, organic matter (smoke, < 2.5 um) and dust (< 10 um). This is where the fires and dust sources put particles into the air, and how much. **Not used, and it should be:** a "Fire and dust sources" chart (the flux on a log scale over the terrain, the sources being points and plumes), and a layer for the master map beside the smoke. It is the start of the plume the other fields carry downwind.
- **`MASSDEN` at 8 m:** organic matter (< 2.5 um), dust (< 2.5 um and 2.5 to 10 um), and Total aerosol twice, as the hour's mean (11-12 hour ave), < 2.5 um and < 10 um. The outlook asks for "Total aerosol" without the size, so which of the two it gets depends on the matching; it should ask for the < 2.5 um one (PM2.5, which the EPA index is for). A "Near-surface smoke and dust (PM2.5)" chart, and PM10 for dust storms.
- **`COLMD`, entire atmosphere:** column mass of the smoke and the dust (the haze aloft, the red sun even when the ground air is clean). A "Smoke aloft" chart.
- **`AOTK`:** aerosol optical thickness (how hazy the sky looks; comparable with the satellites' AOD). A chart, and a check against GOES AOD.

**Steps:** (1) the outlook's want gets the size (`aerosol_size <2.5e-06`) so the PM2.5 record is read for sure; (2) charts for the RRFS: emission flux, near-surface PM2.5 and PM10, smoke aloft, optical thickness, with the US AQI bands as the scale for PM2.5; (3) the master map layer of the emission flux (the sources) under the smoke; (4) the extended point view's air quality line from the RRFS PM2.5 at the place for the next 3 days (AirNow's forecast stops at about 5 days, for the cities only).

**Built (2026-11-16), steps 1, 2 and 4, step 3 in the model layer:**
1. `GfsGrid::IdxRecord::more` keeps the index field after the detail (the size); a want's detail `<detail>|<more>` asks for both (`GfsGrid::find`; `tests/gfs/gfs_test.cpp`: the two Total aerosol records told apart). The nice weather outlook now asks for `aerosol=Total aerosol|aerosol_size <2.5e-06` (it got the PM2.5 record before only because it is listed first). COLMD, AEMFLX and AOTK are read with `GRIB_NORMALIZE_UNITS=NO`, as MASSDEN was.
2. Nine RRFS charts in "Air quality and smoke" (`GfsProductsRrfs.cpp`, RRFS only, with 10 m barbs): `pm25_sfc`, `pm10_aq` (US AQI colors, clean air under a third of "good" left clear), `smoke_sfc`, `dust_sfc`, `smoke_column`, `dust_column` (mg/m2), `smoke_emission`, `dust_emission` (ug/m2/s; a source is one 3 km cell, drawn over about 10 km, the largest value near each cell) and `aod`. The two smoke charts paint the fires (emission flux of at least 0.1 ug/m2/s) over the smoke as an overlay with its own legend row. Drawn from the 06Z run of 16 Nov (f012): PM2.5 to 112, PM10 to 351, fire flux 3.3 (a quiet day: small burns), dust flux to 471 (7,189 after the widening takes the largest near each cell), smoke column 4.3 mg/m2, AOD 0.28. The alias table and the catalogue dump were regenerated.
3. The master map: these charts are in the model layer's list for the RRFS (the layer draws a chart's overlays, so the fires come with the smoke charts); **not seen on the map** (the map's headless export cannot draw a model chart here).
4. The extended point view: a line "Smoke+dust ug/m3" under the precipitation, from the newest 6 hourly RRFS run (the one before when the newest has nothing yet), at the columns' times every 3 hours to 84 h (`GfsRender::pointSeries`: one field over a 3 degree box round the place, by hour; `ForecastPointExtended::addAirQuality`). Seen at Norman (RRFS 12Z: 0.0 to 0.4). **Found:** the RRFS models smoke and dust only (no traffic or industry), so clean air reads near 0 and the number is not the place's PM2.5; the label says so. The nice weather smoke cap reads the same field, so it caps for smoke and dust only (as meant).
Left: a picture of the charts on the master map, colour keys for the fires on the map's legend, the HRRR's own smoke fields (its files have MASSDEN and COLMD too), checking the RRFS PM2.5 against AirNow observations.

**Also (2026-11-16): runoff and the parcel's level.**
- **`SSRUN`, the RRFS's storm surface runoff** (each file its own hour, "11-12 hour acc", kg/m2 = mm): two RRFS charts, `runoff_1hr` and `runoff_6hr` (the hours added up; from hour 1 when the run is younger), the precipitation scale, under 0.05 mm clear. 06Z of 16 Nov, f012: the Pacific Northwest coast and the Sierra, at most 1.7 in in an hour (one cell; not checked against gauges). No `BGRUN` (baseflow) in the files.
- **`PLPL`, the pressure the most unstable parcel is lifted from** (255-0 mb above ground; the GFS, the HRRR and the RRFS all have it): two overlays in the chart's tick list, `line_plpl` (Lines, every 50 mb) and `fill_plpl_elevated` (Fill: the surface pressure less the parcel's, clear under 50 mb, so elevated convection shows), both only where the same parcel's CAPE (255-0 mb) is at least 100 J/kg (with none, the "most unstable" parcel is any level and filled the map). Seen over the 850 mb humidity chart of the three models (06Z, f012: the Gulf coast and south Texas, mostly surface based). Not in the model clones (GEFS, ...).
- **Ceiling (2026-11-16):** `ceiling` for the nine analyses (RTMA, URMA and their AK, HI, PR, GU variants: `CEIL`, every one has it; "no ceiling", 52,493 ft, left clear) and four REFS chances, `ens_prob_ceil_500ft` / `1000ft` / `2000ft` / `3000ft` (the ensemble prob file's `HGT cloud ceiling prob <152.4 ...`). No REFS mean ceiling: the mean file averages members with and without a ceiling and drew 1,000 to 5,000 ft over the clear desert. **Fixed on the way:** every REFS chance chart (the prob and eas files: frost, CAPE, the 1, 3 and 6 hour rain and snow chances) multiplied by 100 a field that is already percent (maxima of 10,000); now read as it is.
- **Clouds by height (2026-11-16):** `cloud_layers` ("Cloud Cover by Height"), one map of the low, middle and high cloud cover (LCDC, MCDC, HCDC): each place in the colors of its layer with the most cover (gray low, blue middle, pink high; a tie to the lower layer; under 20% clear), the low layer the fill and the other two overlays with legend rows (`cloudLayersProduct`, GfsChartCommon.cpp). For the GFS, the HRRR and HRRR-AK, the RRFS and its AK, HI, PR, NA and FIREWX runs. **Who else has the layers (checked 16 Nov 2026), not wired yet:** the IFS, IFS-ENS and AIFS (open data `lcc`, `mcc`, `hcc`: the ECMWF index names need mapping); the Open-Meteo models ICON, UKMO, GSM, ARPEGE (world and Europe), GRAPES (also cloud at 30 pressure levels), HGEFS, AROME (France, HD), MSM and ICON-2I (`cloud_cover_low/mid/high`; GDPS has the total only; AROME-AT's list did not answer); CFS-ANL (as TCDC on "low/middle/high cloud layer", not LCDC). Not checked: ERA5 (lcc/mcc/hcc), GEOS-FP (CLDLOW/CLDMID/CLDHGH), ICON-D2 and ICON-EU (DWD clcl/clcm/clch), HRDPS, GEPS. The total only: GEFS, WOOF-NAM, NBM (total and high).
- **Cloud layers wired for the others (2026-11-16, later):** `cloud_layers` is a GFS chart cloned onto every model that lists LCDC, MCDC and HCDC (26 models now). IFS and AIFS (the ECMWF index names `lcc`/`mcc`/`hcc` -> LCDC/MCDC/HCDC in `parseEcmwfIndex`; the IFS gives fractions, scaled by 100; **the AIFS gives percent**, so no scale), ICON, UKMO, GSM, ARPEGE, ARPEGE-EU, GRAPES, HGEFS, AROME, AROME-HD, MSM, ICON-2I (Open-Meteo `cloud_cover_low/mid/high`), ICON-D2 and ICON-EU (DWD `clcl`/`clcm`/`clch`), GEOS-FP (`CLDLOW`/`CLDMID`/`CLDHGH` from `tavg1_2d_rad_Nx`, fractions), ERA5 (`128_186_lcc` ... monthly files). All drawn and looked at except ERA5. IFS-ENS has the index names but makes its own charts (no clone). Left out: CFS-ANL (its cloned charts are held to the fields NCEI's monthly files have since 1979), HRDPS and GEPS (MSC files have the total only), GEFS, WOOF-NAM, GDPS (total only).
- **AROME-AT removed (2026-11-16):** Open-Meteo keeps no map files of GeoSphere's AROME any more (`data_spatial/geosphere_arome_austria` empty every month of 2026, `latest.json` 404); only its point series remain.
- **Found: the decoded-grid cache did not know the value scale** (the values are kept scaled, the name had no scale), so a scale change served old grids (the AIFS layers at 10,000 after the scale was taken off). The GRIB path's cache name now carries `_x<factor>` when the scale is not 1 (`GfsData.cpp`); scaled fields read once more after the update. The other readers (om, netCDF) were not checked for the same.
- **Found, not fixed: ERA5's surface fields do not load** ("ERA5 does not have TCWV / LCC for ..." on 2022 and 2024 dates, no request is made; the monthly files are in the bucket). Its pressure level charts were not tried. The existing `mslp_pwat` fails the same way, so this is older than the cloud layers.
- **Fixed (2026-11-16): the models whose address is made in code ignored it.** Since the server names (section 20), a model file whose url names a server gets a list of candidate addresses (`GfsModelFile`: `candidates`, for falling back from one server to the next), and `GfsData::urlsFor` asks that list before `fileUrl`. Six models make or change their address in code (`GfsModels.cpp`), so the code was never asked: ERA5 and CFS-ANL asked for addresses that do not exist (no field at all: "ERA5 does not have TCWV"), RAP-ANL lost the newer NCEI folder (May 2020 on), RTMA and URMA lost the older file endings (before May 2017), GFS1-ANL lost the GRIB2 ending (April 2017 on), and the RRFS runs between the 3 hourly ones lost their 15 minute files. Now the four that make the whole address drop the list (`candidates = nullptr`), and the two that change it (GFS1-ANL, the RRFS) change every candidate the same way, so the servers still fall back. Drawn: ERA5 MSLP and precipitable water and 500 mb (2022), the cloud layers (2024), CFS-ANL 500 mb (2010 and 2026), RAP-ANL (2015, 2021), GFS1-ANL (2019), RTMA (2015), RRFS 07Z f003. No automatic test of it yet (every case reads the network).
- **(2026-11-16, after):** the decoded-grid cache names carry the scale in the whole-file reader (Open-Meteo, NetCDF, MSC, DWD) and the GFS box reader too (`x<factor>`), as in the indexed path; ICON's sea level pressure (x100) read again through it (102,510 Pa). `tests/gfs/models_test.cpp` checks, for every model with a candidate list and runs of 2010 to 2026, that the first candidate is `fileUrl`'s address, and pins the ERA5, CFS-ANL, GFS1-ANL and RRFS in-between addresses (it fails on ERA5 with the fix undone). **The HRRR's smoke** (HRRR and HRRR-AK, the HRRR-Smoke fields of `wrfsfc`: MASSDEN at 8 m, COLMD, AOTK; no dust, no emission flux): `smoke_sfc`, `smoke_column`, `aod` (06Z f012: a plume aloft over Idaho and Montana, 13 mg/m2; near-surface smoke to 34). Section 24 left: the picture on the master map, AirNow against the RRFS PM2.5.
- **Section 24 finished (2026-11-16):** (1) **seen on the master map:** `mcexport` with the RRFS `smoke_column` and `pm25_sfc` (a private settings folder, the 06Z run pinned with `MM_MODELCHART_RUN`): the map draws them, the fires over the smoke with the "fires" row in the legend (the export route works when the run is pinned; it did not find a run by itself before). (2) **The RRFS PM2.5 against AirNow:** the hourly observation file (`files.airnowtech.org/airnow/today/HourlyData_2026111614.dat`, 814 PM2.5 monitors in the contiguous states, with `Monitoring_Site_Locations_V2.dat`) against the RRFS 12Z f003 and 06Z f009 hour means and GEOS-FP's all-species PM2.5: on this clean morning the monitors read a median of 4.6 (70 over 12, 2 over 35: Missouri, Indiana, Illinois, morning inversions and local sources), **the RRFS 0.01** (bias -5.9, r 0.01, rank r 0.3: it models fire smoke and dust only, so outside smoke it says nothing about the air), GEOS-FP 4.6 (bias -1.0 but r 0.07: the level right, not the places). So: the RRFS number is a smoke and dust signal, as labeled ("Smoke+dust"), not the air quality; the nice weather cap is right to use it only as a smoke cap, with AirNow's own forecasts for the cities. **Not tested:** a day with smoke (the only case where the RRFS should match); worth repeating on one. The comparison is a scratch script, not in the app.
- **Section 10, the world export (2026-11-16, built, not yet seen):** `WorldRender::renderWorld` takes `over` (another model's sources, their ramps made by `WorldRender::overRamp`: alpha at most 215, clear under the lowest sixth, as GfsRender's cross fill) and `fadeBase` (the chart's own fill at three tenths); `MasterMapViewer::exportWorldTo` loads the model layer's `crossFill` chart for the same time, passes it, names it in the runs line and adds its legend row; the history pictures go through the same export. **Left:** a headless check (the `modelat` route resets the cross fill through `openModels`' `ChartSelection`: give the route a `WXQT_CROSS` that builds a selection with it), then the compare tiles.
- **Section 10 complete (2026-11-16):** (1) the whole-world export: seen on a north polar picture (ICON 2 m temperature over the faded GFS 500 mb winds: the runs line names "ICON ... (over)", the legend row "ICON ... (over)"); an Equal Earth export without "High resolution" takes the map layer's own picture, which draws the second field too (seen: the first picture of the map layer's cross fill). `modelat` takes `WXQT_CROSS` / `WXQT_CROSS_FADE` (a `ChartSelection` for `openModels`). (2) **The compare tiles:** every tile of a plain chart gets the main chart's second field for its own valid time, its frame key with it (`makeJob`); seen with "Compare: models (same hour)" (GFS with ICON 12Z, IFS with ICON 06Z). (3) **Found and fixed: the Model Viewer's main picture never drew the second field** (`reload()` drew from the viewer's `variant`, which does not have it, and kept the picture under a key that says it does; the button named the field): now the main request carries it. `ModelViewer::setCrossFill` (what the dialog does) and `WXQT_CROSS` on the `modelpicker` route. **Left, small:** with a second field the map layer gives no legend row for its own chart (the whole-world export shows only the second field's row).
- **Section 23, two of its left items (2026-11-16):** (1) **the Why? maps at the place:** `ModelCompareViewer::showMaps` takes the place and opens the Model Viewer on a local area round it, 3000 km across (`GfsChart::localId`, "the place"), from the comparison window and the bias window; seen: GEFS mean 500 mb for Norman with "the place" marked. (2) **a station the person picks:** the bias window has a station menu beside "Judged on" (shown when a station judges): "Nearest station with the days" (as before) or one of the 8 stations near the place, filled from each load (`ModelBias::fetchStation`'s `wanted` and `nearby`; `Station` has its lat/lon now); a picked station is judged alone and the forecasts are read at its own place (Will Rogers picked: the IFS 1.9 F too low against 1.4 F read at Norman). The menu lists stations that ever had the elements, so some have no recent days (Tinker AFB and Wiley Post: 0 of 31; the message says to pick another). Headless: `WXQT_STATION_PICK=<n>` on `modelbias` (prints the menu). **Left:** the wind and other variables at stations (ACIS has them for first order stations only), the ensemble means instead of the deterministic runs (the previous-runs service is deterministic only).
- **Section 8, precipitation type (2026-11-16):** the extended view's precipitation bars are split by the type the bulletins give (PRA, PZR, PPL, PSN: the chance of each if it falls), rain at the bottom, then freezing rain, sleet and snow, with a key beside the graph; a column without the types (the NWS grid's) is all rain-green (before: blue when PSN was over 50, else green; freezing rain and sleet never showed). Seen at Steamboat Springs (an 81% column part rain, part snow; the snow days blue) and Marquette (rain only). The station per saved place: not needed (the nearest stations are one request at each load). **Left:** the thumbnail map, the NBH (25 hourly columns where the NWS grid's few are now: asked, 2026-11-16).
- **Section 8, the map beside the table (2026-11-16):** a click on a column draws the GFS's "MSLP, 10 m wind and precipitation type" (`10m_wnd_precip_type`, it reaches all 11 days) for that time on an area 1500 km round the place (`GfsChart::localId`, "the place" marked), 440 px, beside the table, off the screen's thread (only the newest pick is shown); a click on it opens the heights and winds as the button does. Seen at Steamboat Springs, Wed 18 Nov 12Z (the GFS 12Z run, F048: the snow showers over the mountains). Headless: `WXQT_EXTCOLUMN=<n>` (and `WXQT_EXTCOLUMN_MS`, after the page has loaded: a load resets the pick) on `extpoint`. **Left:** the NBH (asked, not answered).


## 26. IFS precipitation by type (2026-11-17, the user: "IFS precip by type, similar to the GFS selection")

The IFS's open data has `ptype` (one field of codes: 1 rain, 2 thunderstorm, 3 freezing rain, 4 mixed or ice, 5 snow, 6 wet snow, 7 rain and snow, 8 ice pellets, 9 graupel, 10 hail, 11 drizzle, 12 freezing drizzle). `parseEcmwfIndex` now gives it as the four flags NOAA's files have (`CRAIN` 1, 2, 10, 11; `CSNOW` 5, 6; `CFRZR` 3, 12; `CICEP` 4, 7, 8, 9), each record carrying the codes that count (`IdxRecord::codes`, read as yes or no where the grid is decoded), and the IFS clones them (`clone_variables`), so the GFS charts that read the flags now exist for the IFS with no chart of their own: `precip_type` (MSLP and precipitation, rain / snow / mixed), the four `*_type` thickness, 850 mb temperature and 10 m wind versions, and `precip_rate_type` (6 charts). Seen: Wed 18 Nov 06Z, snow over the northern Rockies, a pink mixed strip, rain green. The code table is from memory of ECMWF's (`ptype`, param 260015): check it against a place where the type is known when one comes up (hail counts as rain, graupel as mixed). Test: `tests/gfs/gfs_test.cpp` (the four flags and their codes from an index). **Left:** the AIFS has no `ptype` in its open data (its `ptype` would be the same code); the IFS-ENS probabilities of type; the `sf` (snowfall) field the IFS also has, for a snow amount in place of the precipitation by the type.
## 25. The Gross to Miserable weather index (2026-11-16)

From the user's specification (`gross-weather-index-formula.md`, 2026-11-16): the nice outlook turned over, a score of the day's discomfort from 0 up, in four tiers (1 Mild 0-19, 2 Gross 20-44, 3 Nasty 45-69, 4 Miserable 70+). **Built under the nice weather framework** (`GfsProductsNbm.cpp`, the NBM by day, hour 24 = day 1, the daytime hours of each place by the sun, the same mask), wrapping the nice outlook's scoring so that chart is unchanged:
- each daytime hour (9 to 19 by the sun) scored: feels like (the blend's apparent temperature) 1.5 a degree F over 85 or under 45; dewpoint 2.5 a degree over 65; precipitation likely (the hour's chance 50% or more) 15, or 40 when freezing rain or sleet is the likely type (the blend's PTYPE chances, `prob >=3 <4` and `>=8 <9`); chilly dampness (2 m temperature under 50 F with RH over 85%) 15; sustained wind 1.2 a mph over 15; feels like 105 F or more, or -10 or less, 50. The day takes its worst hour; the daytime smoke over 55.4 micrograms (AQI over 150) adds 35. **Left out:** the spec's active severe weather warnings (a forecast day has none; the chance of thunder is not severe weather).
- **`gross_weather`**: the tiers (Mild clear), the score in the hover. **`nice_gross_weather`**: one map, a place either its nice level (1-5) or its gross tier (6 Gross, 7 Nasty, 8 Miserable), never both. **Colors kept clear of the nice ones** (greens, teal, Perfect's gold): Gross peach `#f3c6a5`, Nasty orange-red `#e8603c`, Miserable deep red `#a8141c` (the spec's yellow would sit beside Perfect's gold and Decent's pale yellow-green).
- Seen: Mon 16 Nov (12Z run, day 1): Gross where it feels under 45 F (the northern Rockies, the Great Lakes, inland New England), Nasty and Miserable on the high peaks; Thu 19 Nov: the cold, snowy West Gross to Nasty, the East Nice.
- ~~Next, if wanted: the spec's section 5, the thresholds against the place's normal~~ **done, below.** The Nice Weather window (its Day 1-8 combo) shows the nice outlook only; the two new charts are in the model screen's NBM list.
- **Meh days (2026-11-16, the user: "meh blank days", Decent only):** on `nice_gross_weather` a place that is not Gross and is Decent or below shows blank ("meh"; value 1, the scale starts at Fair, the hover says "Meh"); Fair to Perfect keep the nice colors, the gross tiers theirs. Against the reference outlook's cities (its 06Z files of 16 Nov, matched by city and date): Monday 16% meh, 70% nice levels, 13% gross; our meh cities are his none 706, Decent 1172, Fair 1464 (none of his Nice); of his Decent-or-none cities, 59% are meh on ours, 31% a nice level, 11% gross. Tuesday 11% meh; his Decent-or-none: 47% meh, 31% nice, 21% gross. The gross side still takes ordinary cold (the spec's fixed 45 F), so some of his Fair cities show Gross: the normals question (above) is open.
- **Hot and cold lines from the normal (2026-11-16, the user: "a little bit of normals for hot cold, but anything over 100"):** the nice outlook's per-cell normal high (NCEI normals, else the model mean + offset) is kept as a grid (`normal_high`, degrees F); the gross feels-like lines move **half as far as the normal high sits from 70 F**: cold line 45 + (normal - 70)/2, hot line 85 + (normal - 70)/2 but **never above 100** (feels like over 100 counts everywhere). A place with a 40 F normal high: cold line 30 F; Phoenix in July (106): hot line 100. Places without a normal keep 45/85. Dewpoint, rain, chilly-damp, wind and the extremes (105 / -10 F) unchanged. Against his cities (same files): Monday our Gross cities 3242 -> 160 (of his Fair 2706 -> 18, Decent 162 -> 2, the rest cities he leaves unrated); Tuesday 2441 -> 706 (his Fair 976 -> 10, Nice 409 -> 0, Decent 358 -> 106). What stays Gross is the high Rockies (cold and wind on the peaks). **The extreme rule stays at a fixed 105 F everywhere** (the user, 2026-11-16: "105 for those, Death Valley is literally called Death Valley"): where the normal high is 100 F or more (Death Valley, the Coachella Valley, Yuma, Phoenix, Las Vegas, Laredo), an ordinary summer afternoon of 105-110 F reads Nasty (+50, plus 1.5 a degree over 100), on purpose; 100-104 F there stays meh or Gross by its score.
- **Dewpoint against his sub-scores (2026-11-17, nothing changed):** his per-state files carry his dewpoint and its sub-score for every city: the curve is a function of the dewpoint alone (50 at 30 F and under, 4 a degree to 90 at 40, a point a degree to 100 at 50, 100 to 60, then 6 a degree down) and ours matches it. Our daytime mean dewpoint, set against his, agrees within 1 F from 36 to 56 F (his is 1.5-3 F lower above 56 F, higher below 30 F: regression to the mean, sd 3-4 F); that scatter is what costs levels (12 score points a 3 F miss on the dry slope). His Beautiful cities' dew scores run 80 (1%) to 92 (median): our mark of 92 excludes half of them, but lowering it over-calls: on his 21,093 cities day 1, mark 92 / 88 / 85 / 82 = 61.6 / 60.8 / 57.1 / 55.7% same level (Beautiful 166 / 430 / 1,622 / 2,192 against his 1,153), and a calibration of our dewpoint to his (dry +, moist -) took it to 60.4%. On 8,700 cities of 15 states, three days, the best mark moves day 1 by +1.6 points, day 2 by 0 and day 3 by 0 (day 2 over-calls Beautiful, 600 against 459). So the dewpoint is not the lever; the gates (every ingredient at its mark) can not follow his weighted score from noisier inputs.
- **AirNow kept by valid day (2026-11-17):** `AirNowLoader` read the rolling `today/reportingarea.dat` (20 minutes) for any run, so a redraw of an old run changed its air-quality caps as the file moved on (about 800 cells at Fair on 17 Nov). Now a day that has passed reads AirNow's own file of that day (`files.airnowtech.org/airnow/<year>/<yyyymmdd>/reportingarea.dat`), kept compressed in the cache (`gfs-v3/airnow/<yyyymmdd>.dat.z`, about 120 KB a day, not pruned); today and the days ahead still read the rolling file. Checked: a redraw of the 16 Nov 06Z run is identical, and reads from the kept file.
- **The score as a header, a test, a daily check (2026-11-17):** the hour's score, the normals lines and the tiers moved out of the chart into `src/gfs/GrossScore.h` (no Qt; the chart's values unchanged on 21,093 cities); `tests/gfs/gross_test.cpp` / `run_gross.sh` hold it to the specification's own Python function (the same hours: mild, sticky, heat index 108, cold damp rain, snow, wind chill -15 ...), the 45/85 lines moving with the normal (40 F normal: 30 and 70; 106 F: 63 and the 100 cap), the tier edges, and Phoenix's 102 and 106 F (freezing rain counts 40 instead of 15, the specification's text; its code adds both). **The daily check** (outside the repo, with his data, in `<the kept-data folder>`): `fetch.sh` (cron, 01:40-02:15 and 06:40-07:15 Eastern, keeps only his changed files as `auto/*.tar.zst`) and `check.py` (for each kept file: its run = his update less 5 h to the 00/06/12/18Z cycle, our `nice_gross_weather` drawn headless at his cities for his valid date, a line in `results.csv`: same-level % and within one, his and our Beautiful, our Gross and what he rates them, our meh and what he rates them). Not committed (equation work).
- **The gross side as negatives (2026-11-17, the user):** the combined chart is one scale: **-3 Miserable, -2 Nasty, -1 Gross, 0-2 meh (blank), 2 Fair, 3 Nice, 4 Beautiful, 5 Perfect**; the Gross to Miserable chart is -1, -2, -3 (0 Mild, clear); the raw score is a negative in the hover and the samples (-37). A name for each place stays in the hover (`nice_gross_name`), and the plain nice level is a hover column (`level`, for the daily check). Legend: one block a stop (no closing stop: the Perfect block was drawn twice as wide with one). First daily check (17 Nov, his files, matched by city and date): same level 62.0 / 57.5 / 49.6 / 47.8 / 53.8 / 46.8 / 57.9% for days 1-7 (day 1 and 2 from his 06:31 update, 3 too, days 4-7 from 01:31); our Beautiful against his: 270/1153, 863/491, 26/404, then 0/0 on days 4-7 (his files have no Beautiful that far out): the lead-day bias is real, day 1 under, day 2 over. **Beautiful by definition does not exist on days 4-7** (the user, 2026-11-17; his files have none, and the day cap already holds us at Nice from day 4: 0 of ours on those days, matching). **Beautiful cutoff on our weighted base** (fitted per day on 15 states, 8,100 cities, F1 against his Beautiful): 95.3 / 95.4 / 95.2 for days 1-3, the same each day (the gate marks swing); used for Beautiful alone over the existing levels it moves day 1 from 53.4 to 55.2% same level (782 Beautiful against his 977, ours 266 now), days 2 and 3 unchanged. Not applied; days 1-3 only if it is. **Normals and his raws (2026-11-17):** his scores sit in clusters (Beautiful and Nice both reach 92; 82 and 72 too), and at a score of 90 or more what separates his Beautiful from his Nice is the temperature against the normal: his Beautiful days are 1.5 F under the place's normal high on average (temperature sub-score 92), his Nice 3.2 F over (88). His temperature sub-score is explained 69% by his high alone and 85% with the normal (R2), so the normals do reach the raws; against (his high minus our normal high) he is flat from -5 to +10 F (86-87), 77 at +10 to +15 and 39 at -20 to -15, where ours is 67 and 53: **our warm side is too hard (-10 points at +10 to +15), our very cold side too easy (+15 at -20 to -15)**. Refit `warmCurve` / `coldBelowCurve` to his table next (the normal is a hover column now, `normal_high`). **Done as a trial (same day):** a `normalFix` curve added over the two (his minus ours by high-over-normal, points; leave-one-day-out error 10.3/10.3/11.9 to 9.7/9.5/11.1); same-level agreement on 15 states 51.5/47.1/46.8 to 52.1/47.3/49.0%, Beautiful 161/600/12 to 188/600/57 (his 977/459/123), levels changed 114/100/407 (right now 79/51/286, were right 35/31/93); temperature score rmse 10.3/10.3/11.9 to 10.1/9.9/11.3. In the flat zone (ours 88+) his temperature score runs 80-100 (sd 6.8; 39% from his high over the normal, 46% with the low) against our 94-97. **Our high against his** (hover columns `apparent_high`, `apparent_mean`): our smoothed day's high (feels like) against his `temp_high`: corr 0.956, bias -1.6 F (day 1 -0.5, day 2 -2.0, day 3 -2.5), sd 3.1 F; unsmoothed it is worse (sd 3.5). A 3 F error in the high is about 7 points at the steep part of his curve: most of the 80-100 spread is the two sources' highs, not the curve. Open: calibrate our high to his by lead day (+0.5 / +2 / +2.5 F), then refit the table; the low is not computed (daytime hours only). **TMAX trial (2026-11-17, the user: "try the Tmax"):** the blend's 12 hour maximum temperature (`TMAX`, 2 m) is a record of the 12Z-00Z window only in the runs from 00Z to 12Z (its window ends 24 n less the run's hour: 12-24 for a 00Z run, 6-18 for 06Z, 0-12 for 12Z; none in the 13-23Z runs; TMIN is the 00Z-12Z window, the low), so a want of `forecast = "tmax-day:<n>"` is made into that record in `GfsData` where the run is known and left out for a run without it (`resolvedNeeds`); the nice outlook asks for it (hover `tmax_day`). **Against his `temp_high`** (15 states, 8,722 cities a day): TMAX corr 0.958 / 0.976 / 0.972 / 0.976 / 0.971 / 0.955 / 0.949 on days 1-7, bias +1.6 / +1.7 / +1.0 / +0.8 / +0.5 / +0.6 / -0.8 F, sd 2.5 / 2.2 / 2.6 / 2.5 / 3.0 / 3.5 / 4.1; the sampled apparent high less 1.5 F had bias -0.5 / -2.0 / -2.5 / -2.4 / -2.5 / -2.8 / -5.4 and sd 2.8 to 4.7: **the drift with the lead day is the 6-hourly sampling, TMAX removes it.** Used as it is in the score (`WXQT_USETMAX`, trial, off) it is worse (temperature rmse 12.8 / 16.2 / 19.1 against 10.3 / 10.3 / 11.9; same level 51.6 / 43.3 / 40.6% against 52.5 / 47.1 / 46.8%), because the warm and cold curves and `normalFix` were read off the apparent high less 1.5 F (TMAX is 1-2 F higher on days 1-3): **they must be refit on TMAX over the normal before it is turned on.** The blend has no maximum dewpoint or apparent temperature (only TMAX, TMIN, MAXREF, FOSINDX), so an apparent high would be TMAX plus (apparent less temperature) at the peak sample, which needs the hourly 2 m temperature. **The TMAX refit (same day):** the high term refit on TMAX over the normal (his score less a quarter of the hours' range score, binned by TMAX minus the normal, for the 8,700 city days that are free of the absolute cold term: 70 at -9 F, 79 at -5, 89 at -1, 93 at +1, 95 at +5, 82 at +9, 75 at +11) gains nothing: held-out temperature rmse 12.2 / 9.6 / 11.6 against 10.1 / 9.9 / 11.3 for the current curves with `normalFix` (free subset: 7.5 against 7.3). Days 4-7 (00Z run, his 01:31 files): rmse 13.6 / 14.2 / 14.6 / 15.7 with the apparent high and `normalFix`, 15.0 / 14.4 / 17.0 / 18.2 with TMAX in place (same level 46.7 / 61.3 / 45.9 / 54.7% against 41.8 / 59.6 / 46.0 / 54.7%). **TMAX measures his high better, but does not predict his score better: the scoring is calibrated on the apparent path, and the remaining error is where the absolute cold term binds (64% of November city days: our cold-curve score is 6-12 points over his at an apparent high of 44-62 F).** The score stays on the apparent high (the trial switches are removed; `normalFix` is on; the hover columns `tmax_day`, `apparent_high`, `apparent_mean`, `t_range`, `normal_high` and the run-aware TMAX want stay). **The apparent against the lead day (the user's point):** our apparent high over his apparent is 5.1 / 3.0 / 2.4 / 1.7 / 1.0 / 0.3 / -1.3 F on days 1-7, not the fixed 5.5 the cold term assumes; using the day's offset changes nothing (temperature rmse 10.1 / 9.9 / 11.3 / 13.6 / 14.2 / 14.6 / 15.7 to 10.2 / 9.8 / 11.2 / 14.0 / 14.1 / 14.9 / 15.3). **Even from his own inputs his temperature score is not a clean function:** his apparent alone explains 67% (residual sd 10.7), his high 69%, his apparent and his high over the normal 84% (sd 7.3), adding his low and the crosses 86% (sd 7.0); ours is at 10: about 7 is the floor from what his files show, the other 3 is the two forecasts. Nothing more to gain from the temperature curves; the levels' remaining mismatch is the gates and the inputs. Next, if wanted: TMIN for the low (his score reads it: 96 at a low of 34-42 F, 83 above 58 F). **Daytime cold fit check (2026-11-17, the user):** our temperature score less his by our apparent high (24,000 city-days, days 1-3, current build): +5.5 / +8.7 / +3.4 / 0.0 at 20-24 / 24-28 / 28-32 / 32-36 F (too easy in deep cold, few cities), -3.1 / -5.1 / -4.8 / -2.0 / -3.2 / -3.1 at 36-60 F, +0.1 / +1.0 / +0.8 at 60-72 F; rmse 12-15 at 28-56 F, 7-10 above 56 F. The shape is fine on average; what is there is the lead day: in cool weather (40-56 F) the bias is about +3 on day 1, -4 to -9 on day 2, -6 to -14 on day 3 (our highs run colder than his later in the run). The day's offset for the cold term did not help (above), so no change; the daily check keeps the numbers. **Rain (2026-11-17, the user: "look at the rain curve"):** against his `precip_probability` and rain score: our chance (1.5 times the daytime hours' mean chance, plus 1.5) averages 8.5 / 17.6 / 22.6 against his 9.3 / 18.8 / 21.0 on days 1-3 (corr 0.91 / 0.95 / 0.95), so it is not biased, and `rainCurve` is his curve (his score: 100 up to 5%, 81 at 5-10, 32 at 10-20, 6 at 20-30, 0 over 30). **But we miss rain he has, one way:** where he has 10% or more (7,400 city days) we are under 10% in 1,042 (14%), he is under where we are 10% or more in 170: the mountain West (Colorado 191 of 240 of his rainy cities, Washington 362 of 954, Montana, California, Arizona), his chance there 26% on average against our 4.4%, mostly cloudy days. **Not the method:** smoothing the chance before the curve, or no smoothing, changes the rain-score bias (+3.6) by 0.6 or less and the levels by 0.5 points; the blend's own 12 hour chance of precipitation for the same 12Z-00Z window (`APCP ... 12 hour acc ... prob >0.254`, hover `pop12`, a run-aware want `pop12-day:<n>` made in `GfsData` like TMAX) correlates a little better (0.93 / 0.96 / 0.96 against 0.91 / 0.95 / 0.95; rmse 8.7 / 8.7 / 9.1 against 8.3 / 10.3 / 10.4) and misses the same cities (2.0 / 2.5 / 5.9% against 2.8 / 3.6 / 5.5%): **the gap is his source** (the forecasters' 12 hour chance is higher in the mountains than the blend's), not ours. No change to the score; `pop12` and `rain_chance` stay as hover columns. **A 24 hour chance:** the blend has no 24 hour probability (its longest window is 12 hours, 6 hours after 48 h), so it would be the night and day windows put together; not built. **The daytime hours' highest chance** (`rain_max`, hover) against his chance, corr / rmse / his-10%-and-ours-under-10% by day 1-7: mean x1.5: 0.91 / 8.3 / 15.6%, 0.95 / 10.3 / 12.4%, 0.95 / 10.4 / 14.6%, 0.76 / 14.7, 0.93 / 7.3, 0.91 / 6.8, 0.83 / 6.6; NBM 12 hour: 0.93 / 8.7, 0.96 / 8.7, 0.96 / 9.1, 0.78 / 14.9, 0.93 / 7.8, 0.89 / 7.1, 0.83 / 7.0; highest hourly: 0.97 / 5.8, 0.95 / 10.5, 0.96 / 9.1, 0.79 / 15.6, 0.95 / 8.3, 0.92 / 8.2, 0.82 / 8.8. Day 1 reads like a daily maximum (0.97, rmse 5.8 against 8.3) but not the later days, and the misses stay (10-30% of his rainy cities under 10% with every one): the mountain West gap is his source. **The 10-30% band alone (2026-11-17, the user):** his score by his chance: 39 / 17 / 9 / 3 at 10-15 / 15-20 / 20-25 / 25-30 (his curve; ours by his chance: 50 / 32 / 21 / 14, 10-15 points lenient). Taken apart: our chance in those bins is 11.9 / 16.7 / 21.0 / 25.5 against 12.5 / 17.5 / 22.5 / 27.5 (1-2 points low, worth 8-10 points of score on the steep curve: two thirds of the gap), and applying the curve before the 50 mile smoothing adds 2-4 (the other third; smoothing the chance first takes the all-cities bias from +3.6 to +3.0, the levels +0.1). **By our own chance there is no bias:** ours 10-15 / 15-20 / 20-25 / 25-30 gives his score 39.6 / 21.7 / 8.4 / 4.6 against ours 43.9 / 21.6 / 10.8 / 5.0 (0 to 4 points). So the leniency is the steep curve meeting the scatter in the chance (rmse 8-10 points), as the regression to the mean after choosing the cities by his chance, not a wrong curve; the days 1-4 do run low in this band (our chance 10.9 / 13.7 / 15.2 / 15.5 against his 18-19) and a straight rescale of the chance ruins the dry end (rmse 19-67). No change to `rainCurve`. **Then the step (same day, the user: "a change from 5% to 10% is large ... one step at 5-11, then the curve"):** the best step functions on our chance against his rain score (held out a day at a time, rmse, mean of days 1-7): current 19.3, continuous calibrated 17.7, 3 / 4 / 5 / 6 steps 19.3 / 18.7 / 18.4 / 18.3; the four steps are 0-5%: 96, 5-11%: 60, 11-19%: 30, 19% and up: 3. **Built: one step at 5-11% (score 60; below 5% full marks; at 11% and over the existing curve, capped at 60), applied to the smoothed chance** (`rainScore` in `nice_weather`): held out, rmse 18.9 to 18.4 (the fitted curve above 11% gains nothing more: 18.37), bias at 5-30% from +6.7 to -0.1. On his cities (15 states, 7 days) same level 55.7 / 49.2 / 51.6 / 48.6 / 63.2 / 43.8 / 54.1% to 55.9 / 49.1 / 51.5 / 51.1 / 63.5 / 43.0 / 54.3% (day 4 +2.5, day 6 -0.8), Nice or better the same (3449 / 2731 / 1863 / 1750 / 969 / 309 / 630 to 3452 / 2731 / 1813 / 1752 / 969 / 284 / 618), rain-score rmse 14.1 / 16.7 / 20.8 / 23.6 / 18.8 / 19.3 / 21.6 to 13.0 / 15.9 / 21.5 / 18.5 / 18.4 / 18.9 / 20.9. A day with a 5-11% chance is at most Fair on rain (the rain mark for Nice is 89). **Where his Beautiful is Nice here (2026-11-17, the user: "anything common?", "T-Td?", "humidity?", "dew only"):** 979 of his 1,551 Beautiful city-days (days 1-3, 15 states) are Nice for us, held by the dewpoint in 651 (66%), the temperature in 225, the wind in 68, the sky in 32; they are cool, dry, clear days (his dewpoint 42 F, high 60, cloud 8%, our dewpoint score 86 against his 90) in Texas, Massachusetts, Arizona, Tennessee, 749 of them on day 1. T-Td (AUC 0.55) and the relative humidity (0.54) do not separate his Beautiful from his Nice (flat 16-29% between 10-30 F of spread and 35-70% humidity; under 10 F or over 70% almost never Beautiful, over 30 F never); cloud cover (0.71), the dewpoint (0.66) and his temperature score (0.69) do. Our Nice is 23% his Fair and 14% his Beautiful (kind and harsh both; ours higher than his in 32% of cities, lower in 16%), but his Nice is 28% our Fair, so tightening Nice only trades errors (Nice marks sky 80, dew 76: Nice or better 11,653 to 9,559 against his 12,796). **The dewpoint mark of Beautiful, 92 to 86, was tried and taken back (same day):** on the 15 states it read well (same level 52.61 to 52.81%, day 1 55.9 to 57.4%), but those states hold most of his Beautiful; on all 20,323 cities of his files it is worse: day 1 62.15% at 92, 62.27 at 90, 61.72 at 88, 60.06 at 86 (Beautiful 302 / 459 / 650 / 1,279 against his 1,153, right 204 / 295 / 333 / 381), day 2 58.91 / 58.48 / 57.95 / 57.19%, day 3 flat (52.9%). **The mark stays 92** (90 is a wash). The Nice dewpoint mark (67 to 76) did not carry from days 1-3 to 4-7 either. Lesson: fit and judge on the whole set of his cities, not the 15 states fetched with sub-scores. **His blank is the rain day (2026-11-17, found when the user asked about his unrated cities):** the cities are in his files with a number (score 2.7-81.7) and no category: 8 / 14 / 14 / 16 / 31 / 37 / 19% of his cities on days 1-7, mean chance of rain 65% (58% have 60% or more, against 1% of the rated), weather codes of rain and drizzle, mostly California, Florida, Washington; it is a step under Decent. We have no such level (Decent is the floor, `none` only for the air quality). **Tried offline and in a trial build: no level where our chance of rain is 30% or over** (a cap): same level on all his cities, days 1-7: 63.0 / 58.8 / 52.6 / 55.0 / 55.7 / 45.5 / 57.6% to 68.4 / 68.6 / 61.6 / 64.2 / 68.1 / 49.3 / 60.4%, the best chance between 20 and 25% for days 1-5 (68.6 / 68.7 / 62.0 / 65.3 / 70.1), worse on day 6 (41%). **Built (the user, 2026-11-17: "a chance of 30% or more should be blank or worse"; Meh stays Decent and none; our Gross, Nasty, Miserable count as 1 to 3 levels under his Fair, and a Gross day matters only where he rates it above Decent):** level 0 (none, gate "Chance of rain") where the smoothed chance of rain is 30% or over, a cap in `nice_weather` after the air quality; on the combined map it is blank (Meh) and the gross tiers still come first. On all of his cities, days 1-3, same level 63.0 / 58.8 / 52.6% to 67.7 / 68.6 / 61.7%; with a Gross city agreeing when he is Decent or none, 65.1 / 64.9 / 58.2% to 67.6 / 69.1 / 62.5%; our none 348 / 0 / 3 to 1,632 / 2,175 / 2,579 against his 1,612 / 2,922 / 2,945. **The criteria misses (2026-11-17, the user: "none is the most obvious place to start ... a definition issue?"):** with the 6 criteria (temperature, rain, heat, dewpoint, sky, wind) there are 63 subsets that can disagree (reached marks differ), 32 of 64 seen in the 6,522 misses of 19,241 city-days (days 1-3, 15 states): none differs 18.6%, sky 12.6%, temperature 9.0%, dewpoint 8.7%, temperature + sky 5.8%, wind 5.3% ...; 12 combinations are 83%. **The "none differs" misses are the rule, not the forecast:** with the current build 1,379 of 7,894 (17%): our gate dewpoint 563 (his Beautiful, our Nice 439), wind 213 (his Nice, our Fair 190), sky 198, rain 176 (his none 42% chance of rain against our cutoff at 30%), temperature 161, gusts 42, air quality or smoke 19. His Beautiful allows a dewpoint score down to about 83 and a Nice a wind or sky under our marks: no single mark moves it (86 for the Beautiful dewpoint lost 1.7 to 2.1 points on all of his cities), and **a compensation rule (one criterion a step under, taken when the weighted base is high: Nice at 85-94, Beautiful at 92-96) is worse everywhere on all 60,969 city days (best 61.7% against 66.0% now)**: the base is too high on nearly every day to tell them apart. What is left to match is his rule for combining the criteria, which his files do not show (his scores are a weighted sum, his levels a gate we cannot read from 21,000 cities).
**The Beautiful gap (2026-11-17, the user: "the Beautiful gap is annoying ... we are missing something"):** his Beautiful against our six ingredient scores: it clears our sky mark in 100% of cases (his Nice 81%), temperature 57% (25%), dewpoint 29% (23%), wind 39.5% (31%), all six 14% (0%): dewpoint and wind barely separate it from his Nice; his total score sits in steps of 10 (92 / 82 / 72: his air quality); **his AQI is `max(aqi_forecast, aqi_observed)`** (forecast a model value on days 1-5, close to Open-Meteo's CAMS `us_aqi`, the observation on day 1 only, AirNow-like; PurpleAir is the user's idea for the observation) and gates his Beautiful (11% Beautiful under AQI 40, 7% at 40-50, 3% at 50-55, 2% at 55-70, 0 over 70) but not his Nice; with his exact AQI as a gate our agreement would fall (67.95 to 67.6 / 67.85%: we under-call Beautiful, a gate only removes). **Where we miss it:** his 4,372 Beautiful city days of 16 and 17 Nov (six day-files): we call 12%; Texas 27%, Arkansas 48%, New Mexico 21%, Louisiana 13%; 0% in Maryland, Kentucky, Massachusetts, Indiana, Tennessee, Connecticut, Oklahoma; none from 38 N up; Massachusetts: his sub-scores temperature 87, dewpoint 88, wind 87, sky 100 against our marks 91 / 92 / 94 (his Beautiful is not every ingredient at its mark). A tree on his own numbers: south gusts of 20 mph or less and cloud under 38% (63%, 85% south of 29.8 N), north cloud under 4.5% and above 41.7 N (77%). **A tree on our inputs fitted on 17 Nov falls on 16 Nov (AUC 0.87 to 0.61; with latitude, longitude, normal high 0.93 to 0.70): the pattern is regional and day-specific; two days cannot tell a rule from it.** The daily check now keeps the six scores, base, particles, normal high and chance of rain per file (`samples/`), to fit with a whole day left out when there are 10 or more days.
**UV and latitude (2026-11-17, the user: "UV and latitude are interesting ... a term / slope for UV", then "test it against his levels first"):** his file's other fields (UV index, gust, heat index, low and high temperature, weather code, AQI, smoke, population, latitude) added to his six sub-scores in a fit of his level (leave one day out): UV +0.060 R2 (+2.7 points), latitude +0.055 (+2.3), AQI +0.034 (-1.9: it only gates the top), the rest 0 or negative; **but the left-over of his level after his six scores is not smooth in latitude (+0.17 at 30-33 N, -0.16 at 39-42 N, +0.09 above 42 N) and not consistent by day (+0.40 on day 1 at 30-33 N, 0.03 on day 4); UV follows latitude and cloud (R2 0.85) and has no trend** (UV 3-4 -0.14, 4-5 +0.12). Tested as a term: the UV index estimated from latitude and date (clear sky at solar noon, 12.5 cos(zenith)^2.42; 5.0 / 3.7 / 2.6 / 1.5 at 24-30 / 30-36 / 36-42 / 42-50 N on 17 Nov, his file 4.8 / 4.2 / 3.3 / 2.5) with the cloud from our sky score; a logistic fit of his Beautiful against Nice among our Nice or better: AUC 0.752 to 0.788 on 17 Nov, 0.739 to 0.758 on 16 Nov (+0.02); the Beautiful marks of temperature, dewpoint and wind moved by -k (UV - 3): same level 17 Nov / 16 Nov k = 3: +0.15 / -0.19, k = 6: +0.07 / -0.29, negative k worse on both: **no consistent gain; not built.** His Beautiful has a higher estimated UV (3.26 against 2.54 for Fair and 2.57 for Nice), a regional and day effect (the Texas high-pressure day).
**UV as a hump (the user: "0 is bad, medium is good, high is bad"):** the share of his Beautiful among his Nice and Beautiful cities (where we say Nice or better) by the estimated UV rises, no hump in what November gives (it does not pass 5): 17 Nov UV 1-2 5%, 2-3 5%, 3-4 32%, 4-5 30%; 16 Nov 0 / 8 / 25 / 54%; the falling side (UV over 8, summer) cannot be tested now. The Beautiful marks of temperature, dewpoint and wind raised by c x ((UV - u0)^2 - 1) (u0 = 2 to 5, c = 0.5 to 4): same level 17 Nov / 16 Nov within +0.15 / -0.1 of 66.29 / 64.08%: **no gain; not built; revisit in the summer with the daily samples.**
**UV inside the sky criterion (the user: "add it in the sky criteria?"):** the sky score lowered by k x max(0, u0 - UV) (u0 2 to 4, k 5 to 40) or multiplied by min(1, UV / u0): same level 17 Nov / 16 Nov from 66.29 / 64.08% to 65.3 to 66.1 / 63.7 to 64.5% (u0 4, k 5: 65.34 / 64.47), Nice or better 24,245 to 21,853 (the weak November sun of the north takes Nice cities away, his Nice has them), the multiplied form 46 to 65%; for Beautiful only (the sky mark 89 + k x max(0, u0 - UV)): 66.3 / 64.05%, no change. **Not built.**
**The Beautiful wind mark, 82 to 94 (2026-11-17, the user: "96 wind ... yes wind"):** his Beautiful cities have a wind score of 96 or more in three of four; his own best splits as marks (Beautiful wind 92, sky 83, dewpoint 87, temperature 84; Nice 64 / 65 / 71 / 69.5) lose 2 to 5 points together on both days and every single move is within 0.3 points (a plateau); wind 94 is the one that gained on both days (+0.3 on 17 Nov, +0.2 on 16 Nov, all of his cities). Built; actual runs, same level before to after: 17 Nov days 1-3 67.53 / 68.62 / 61.66% to 67.68 / 69.40 / 61.65%, 16 Nov days 1-3 64.72 / 59.64 / 67.34% to 64.71 / 59.52 / 68.07%; our Beautiful 299 / 885 / 88 to 139 / 600 / 18 against his 1,153 / 491 / 404 (17 Nov) and 28 / 114 / 466 to 13 / 12 / 171 against his 412 / 1,698 / 236 (16 Nov).
**Dry air (2026-11-17, the user: "dry air doesn't really matter to me unless it's proper cold or windy"; then "let's match his first"):** the middle Missouri block (his Fair, our Nice, 168 cities on day 1) is the dewpoint on the dry slope (his 35 F, a score of 68.6 against our 76.5, 4 points a degree: 2 F of dewpoint, while our Nice mark is 67 and 9 in 10 of his Nice score 73 or more). The feel rule, the dry side (under 50 F) not counting, tried against all of his cities: same level 67.5 / 68.6 / 61.7% to 55.7 / 54.7 / 62.5% on days 1-3, 21.5% / 17.8% / 3.3% of the cities go up a level (his levels there Fair, Nice, a few Beautiful), Beautiful on day 1 299 to 4,642 (his 1,153); halved: 59.7 / 59.7 / 62.5%. **Not applied: matching his comes first** (the feel rules are for after).
**Smoothing against his points (2026-11-17, the user: "even smoothing for our display ... that may explain differing scores"):** our ingredients are smoothed over 50 miles before the levels; his published map is an interpolation of his 21,093 city points (3 cities per 10,000 sq km in Nevada and Wyoming, 120 in Pennsylvania, 181 in Massachusetts: broad blobs in the West, fine in the East) on a flatter projection, and his levels are point values. **Tested: our level at his cities, the smoothing radius 0 / 25 / 40 / 80.5 / 150 km** (all of his cities, same level on days 1-3): 65.7 / 65.1 / 59.0, 66.6 / 67.1 / 60.4, 66.8 / 67.6 / 60.7, **67.5 / 68.6 / 61.7**, 64.0 / 67.9 / 61.9%; Beautiful on day 1 501 / 465 / 439 / 299 / 127 (his 1,153). **The smoothing does not explain the scores: 50 miles (80.5 km) agrees best, no smoothing costs 2 to 3.5 points;** it stays, for the display and for the levels.
**The Pennsylvania spot was the air quality (the user's guess):** 311 of the 315 Pennsylvania cities that were blank here and Fair for him on day 1 were held by the air quality, from one reporting area, the Mon Valley (an hourly PM2.5 reading of 172, Unhealthy, at noon, within 50 miles of 311 cities); day 1 held 457 cities by smoke or air quality, 348 of them his Decent or better, days 2 and 3 138 and 24 with none or 3 of them his. **Changed: an hour's reading (a row with an index) no longer takes a day away,** it only keeps it from Perfect or Beautiful (an index over 35); the forecast of the day still caps as before (category 3 and over: no level, 2: Fair, 1: Nice). Day 1, all of his cities: Decent-or-better agreement 96.09 to 97.81%, held by air quality or smoke 457 to 96, our blank 1,632 to 1,284 (his 1,612), same level 67.70 to 67.53% (the unblanked cities are Nice or Beautiful against his Fair). Not committed (equation work).

## 27. The drought window's temperature and rain maps, zoomable (2026-11-17, the user: replace the CPC picture "to match the zoomable shapefiles of the drought monitors")

**Today:** the Normals tab of `DroughtViewer` shows NOAA CPC's four-panel PNG of precipitation (`us.4panel.fordrought.briefing.recent.<period>.rain.<total|anom|percent>.png`, `loadPrecip`); it only zooms as a picture. The Drought Monitor tab's `DroughtMap` is drawn from shapes (sharp at any zoom, state and county borders, the area outlined, a read-out under the pointer, and a `ChangeLayer` of one pixel a cell).

**Sources (the user: not IEM if at all possible):**
- **Temperature, and rain as a first step:** the ACIS climate work of the master map (`AcisClimate::read` for the stations' departure from the 1991-2020 normal, `GridAnalysis::analyze` to a 0.05 degree grid over the contiguous United States; the measures `avgt-dep`, `maxt-dep`, `mint-dep`, `pcpn-dep`, `pcpn-pct`; the periods: last 7, 14, 30, 60 and 90 days, this month so far, last month; the NWS colour bands in `AcisClimate::Product::bands`).
- **Rain from NOAA's own MRMS (second step):** the gauge-corrected multi-sensor rain of `noaa-mrms-pds` (`MultiSensor_QPE_24H_Pass2`, 4 MB a day gzipped, 1 km, from 15 October 2020; the app already reads the 1 to 72 hour products, `MrmsLoader`, `UtilityMrms`), added up over the period and kept at 0.05 degrees (a day is fetched once). IEM's MRMS "reanalysis" is built from these same files. MRMS has no normal: the departure and the percent of normal take the ACIS station normal of the period analysed onto the same grid (the other way, a 1991-2020 grid averaged from nClimGrid, is a bigger one-time job). Periods over a year would be partial (the record starts in late 2020).

**Done (2026-11-17), step 1:** `DroughtMap::FieldLayer` / `setField` (the grid as one smooth image under the borders, a key of the bands, the value under the pointer; the map fits its area to the first size the window gets and refits until the person zooms or drags; double click refits) and the Normals tab (`DroughtViewer::loadPrecip`): a `DroughtMap` in place of the CPC picture, the measure (average, maximum and minimum temperature departure, rain departure, rain percent of normal) and the period (last 7, 14, 30, 60, 90 days, this month so far, last month) from the ACIS stations analysed on the 0.05 degree grid; the Area menu outlines and zooms it like the Monitor's map. Seen for the 7 day average temperature (3004 stations). Not done: the rain measures not looked at on the map, the MRMS rain and the longer periods below, and the CPC `precip` URL code (`showPicture` stays for the outlooks).

**Canada (the user, 2026-11-17: "I know MRMS has at least the higher population areas"):** MRMS's CONUS domain (about 20-55 N, 130-60 W) takes in the populated strip of southern Canada, with the Canadian border radars; the gauge correction of Pass2 is US gauges, so there it is mostly radar (where it rained, less sure of amounts). The totals need nothing extra; the departure and percent of normal need Canadian 1991-2020 normals (Environment and Climate Change Canada, a loader of its own; the ACIS normals are US stations). Order: the MRMS totals first (the user: "Let's start with the totals"), Canadian normals later if wanted. Other domains of `noaa-mrms-pds`: `ALASKA`, `HAWAII`, `CARIB`, `GUAM`, each with `MultiSensor_QPE_24H_Pass2`; the station maps could also take the ACIS Alaska and Hawaii regions (`AcisClimate::regions()` has them; the tab reads only the first, the contiguous United States, and its field layer holds one grid).

**Done (2026-11-17), step 2, the totals:** `src/drought/DroughtRain.{h,cpp}` (the 72 hour and 24 hour `MultiSensor_QPE_*_Pass2` files ending at 12 UTC from `noaa-mrms-pds`, `gdalwarp -r average` to a 0.05 degree grid 130 to 60 W, 20 to 55 N, each file kept in the `drought` permanent cache as 16 bit twentieths of a millimeter and added up; `latestEnd`, `total`) and the Normals tab's sixth measure "Rain, total (MRMS radar and gauges)" (`DroughtViewer::loadRainTotal`: inches or mm by the person's units, ten bands that grow with the period, the key, the read-out). Seen for the last 7 days (3 files, a few seconds the first time). `WXQT_NORMALS=<measure 0-5>,<period 0-6>` opens the tab on a measure for a grab. Left: the rain over the sea and Mexico is drawn as the radar has it (not clipped to land), the departure and percent of normal from MRMS (needs a normal grid: the ACIS station normals analysed, or Canada's), periods over a year, Alaska / Hawaii / Caribbean / Guam domains.

**Progress and soil moisture (2026-11-17):** (1) the line over the Normals tab's map counts the work while it goes (`DroughtViewer::tickPrecip`: "34 of 123 files read" for the MRMS files, the ACIS reader's own `progressText` for the stations; the label's text is set directly, `Text::setText` adjusts the label's size each time and a line set again and again shrinks to a narrow column; the app-wide "Working - n downloads" label under the window already counts the downloads); (2) **soil moisture, zoomable (the user, 2026-11-17, with CPC's "Last 7 Days SMP" picture):** the Outlooks tab's three soil moisture entries (ensemble, latest day, last 7 days, last 30 days) are now a `DroughtMap` field layer (`DroughtViewer::loadSoil`) from CPC's GeoTIFFs `https://www.cpc.ncep.noaa.gov/products/Drought/Figures/smp/ens.tif`, `ens_7.tif`, `ens_30.tif` (0.125 degree NLDAS grid, 482 x 194, -999 empty, percentile 0 to 100, `VALID_DATE` in the file, a few days behind), the picture's bands (under 2 to 98 and over, the middle 30 to 70 left clear), the value under the pointer; the old labels "change over 7 / 30 days" were wrong (the values are the percentile of the last 7 / 30 days). `WXQT_OUTLOOK=<n>` opens the tab on an entry. Left: the other land models (`noah`, `vic`, `sac`, `mosaic`, each with `_7` and `_30`: the same folder), CPC's leaky bucket rank (`USDM_Products/soil/percentile/daily/w.rank.<date>.tif`, only 1 degree: a 64 x 32 grid, so left as the picture) and the soil moisture anomaly and total grids of the same folder.

**Idea (the user, 2026-11-17, "a kind of thumbnail sheet ... the raw drought / change over 1 week / month"):** a sheet of small `DroughtMap`s in a grid, in the way of the four-model chart: the current Monitor and its change over 1, 4, 12, 26 and 52 weeks (the Monitor tab's comparison list already has them: `comboCompare`, `ChangeLayer`), perhaps with the temperature, rain and soil moisture panels of the Normals and Outlooks tabs; one pan and zoom moving every panel, a click opening a panel large, the sheet saved as one picture. Not started; the panels, their count and the linked zoom to be chosen.
**Generalized (the user: "my brain keeps trying to generalize it into an exporter"):** the shape would be a `Sheet` (a grid of panels with titles, one shared key, a footer, one linked view) over a small `SheetPanel` interface (`title()`, `paint(QPainter&, rect, view)` so it is vector at any size, and `view` as the area and time it is shown at): providers `DroughtMap` (Monitor, change, a field layer) and the model chart (`GfsRender::png` for a model, chart and hour), later a table or a point graph; outputs the on-screen sheet, PNG and vector PDF (`ChartExport`'s painting), and a loop of the sheet through time into the animation exporter. Already there: `ChartExport` (one widget to PNG / PDF / clipboard / animation), `ExportDialog` (a kind and its options), `WorldRender` (the whole-world export), the compare tiles of section 10. Missing: the composition (the grid, the shared key and titles) and the panel interface. **The user's second consumers (2026-11-17): "Satellite + Models ... (And outlooks...)"**: the master map already holds all three as layers (the GOES, GIBS and EUMETSAT satellite layers, `ModelChartLayer`, the outlook layers of `SevereAreasLayers`, `OutlookHistoryLayer`, `CfsOutlookLayer`, `CpcPrecipLayer`, with MRMS radar), so the panel source for them is a **master map view**: a layer set, an area and a time, rendered the way the headless `WXQT_OPEN=mcexport` already does; a sheet is then N such views on one area and one time (a severe day: GOES visible, the SPC outlook, an HRRR chart, MRMS), saved as a named sheet like the Checks presets. Two ways to get there: an **export-only sheet** first (each panel rendered by the existing export path and composed into one picture, the view and time set once for the sheet), which costs little; a **live linked sheet** of small map widgets (pan and zoom moving all of them) is heavy (each master map is a whole viewer with its own layer state) and would come after, if at all. The drought maps (`DroughtMap`) are their own widget, not master map layers, so they are a second kind of panel (or become layers of the master map). **The middle route (the user, 2026-11-17: "select area/zoom, then rerendered when the layout is selected"):** one live map sets the view (area, zoom, time); a layout (a grid and which layer set each cell holds, a saved sheet) is then picked and the panels are rendered for that view, one picture each, shown as they finish and kept by (layer set, view, time, cell size) so changing the layout renders only the panels it has not got; changing the area or time renders them all again; the view is fitted to each cell's shape (the extent kept). No linked live panels. The sheet window shows the result (zoomable), saves PNG / PDF, and goes back to the map to adjust the view. Panels are master map views (the headless export path) or a `DroughtMap` painted at the cell's size. **Sheet from the view (the user: the master map is meant to be the big screen view):** the master map keeps its screen as it is (no sheet controls on it; one entry in its existing menu, or a key, "Sheet from this view"); that opens a **separate Sheet window** (it can sit on another screen) seeded with the view: the area, the time and the layer stack as the first panel. The panels are drawn by the headless renderer, never by the live map, so the big-screen map keeps running untouched while a sheet is made. The seed is one small struct (a view: area, time, a layer set), so the Model Viewer and the drought window can hand a view over the same way.
Order to keep it honest: the drought sheet first, written against `SheetPanel` from the start, then a second consumer (the model charts side by side, as the four-model chart) before anything is called an exporter.

**Steps:** (1) done above: `DroughtMap` gets a field layer (like `ChangeLayer`: the cells, a band table, the unit, the read-out; drawn under the borders) and the Normals tab draws the ACIS grid for the chosen measure and period instead of the PNG, with temperature as well as rain; (2) the rain from MRMS with the ACIS normal; (3) longer periods (a monthly source), if wanted; the four-panel view of the PNG (four linked maps) only if asked.

## 28. The sheet: a template of panels over one view (sketch, 2026-11-17; nothing built)

The user's idea (thumbnail sheet, then "my brain keeps trying to generalize it into an exporter"): see the notes of section 27. This section is the **template** first, the thing to read before any code: what a sheet is made of, and the cases that would break it.

### The parts

```
SHEET   name, orientation, size (px or paper), background (light / dark), footer line, key mode
 VIEW    the one view the panels share: area (box or place + km), time (a valid time), projection (flat / polar / globe)
 LAYOUT  a grid of CELLS (rows x columns, with spans: a cell may take 2 columns), the gaps, the title bar height
 CELL    kind (map | drought map | model chart | picture | text | table), title, the layers or chart it shows,
         its own overrides of the view (a different area, a time offset, a zoom lens), its key (own | shared | none)
```

### Orientations and sizes to fix (the first decision)

| sheet | shape | use | cells |
|---|---|---|---|
| Wall | 16:9 landscape | the big screen, a second monitor | 2 x 2, 3 x 2, 1 + 3 (a big cell and a column of three) |
| Page | 8.5 x 11 portrait / landscape | print, PDF | 2 x 3, 3 x 2 |
| Phone | 9:16 or 4:5 portrait | sharing | 1 x 2, 1 x 3 (a column) |
| Square | 1:1 | a post | 2 x 2 |

```
 Wall 2x2        Wall 1+3            Page 2x3        Phone 1x3
+---+---+      +-------+---+       +---+---+       +-------+
| A | B |      |       | B |       | A | B |       |   A   |
+---+---+      |   A   +---+       +---+---+       +-------+
| C | D |      |       | C |       | C | D |       |   B   |
+---+---+      +-------+---+       +---+---+       +-------+
                                    | E | F |       |   C   |
                                    +---+---+       +-------+
```

Each cell has a shape (its pixel width x height); the view's extent is **kept** and fitted to it (a wide cell shows more east and west than a square one; a tall area in a wide cell leaves bands, or the area is cropped: a choice per sheet).

### What is shared and what is per cell

- **Shared by default:** the area, the time, the projection, the title style, the footer, the size of text.
- **Per cell, optional:** a time offset (T-6 h, T, T+6 h: a strip through time), another area (a CONUS cell beside a zoomed one), another model or run, a different layer set.
- **The key:** one shared key where the cells draw the same thing (a model's chart over times or areas); each cell's own key where they differ (satellite, outlook, model) placed in the cell; none for pictures that carry their own.

### Cases to read for (the ones that could break it)

1. **Time means different things** per cell: a satellite frame (observed at), a model (run + forecast hour), an outlook (a day's product: issued, valid), radar (now). One sheet time cannot be met by all: say per cell what time it drew ("GOES 1801Z, HRRR 18Z F00, SPC Day 1 issued 1630Z") and mark the ones that could not match.
2. **A cell with no data for that time** (satellite history is short, a model run not out): the cell shows why, not a hole.
3. **Different projections** (flat charts, a polar map, a globe layer): the view's "area" must mean the same place in each; a sheet in one projection, or a per-cell projection with the area as the common thing.
4. **Raster against vector:** satellite and radar are pictures (they pixelate in a PDF at a large size); charts and the drought maps are vectors. The PDF is vector where the cell is, raster where it is not; say the dpi.
5. **Legend collisions:** two cells with different keys cannot share one; a key under a cell takes its height; a cell with a long key (a drought map's five lines) is shorter.
6. **Fonts and marks at one size** across cells that were designed at different sizes (barbs, numbers on lines, city names): a scale per cell so the text is the sheet's size, not the cell's.
7. **A place marker** (the saved location's ring and name) the same in every cell, or only on one.
8. **Difference: a property of the chart, not a kind of cell (the user, 2026-11-17).** A change since the earlier run, an anomaly, a model against another model's field (section 10's cross fill), the drought map's change since N weeks: each is a chart (or a layer variant) the cell asks for by name, with its own centered scale; the sheet has no "A minus B" cell of its own. A cell is a chart at a view; what the chart is made from is the chart's business.
9. **Attribution:** each source's credit (NOAA, EUMETSAT, Copernicus, a model centre) in the footer or the cell; some sources require it.
10. **Time loop of a sheet:** every cell steps together (the sheet's time through a day) into the animation exporter; the cells' frames must exist for every step or the sheet skips them.
11. **A live sheet: NOT the sheet's job (the user, 2026-11-17: "that's the point of the home page").** The always-current, self-refreshing view of many things is the home page (`HomeThumbnails`, `HomeLayout`, `HomeLayoutEditor`: tiles, each its own refresh). A sheet is a snapshot made on request for a view and a time, so it needs no refresh, no "last updated" line and no cells updating at different rates. Left open only as a later link, not a feature of the sheet: a saved sheet could be a home tile (rendered when the tile refreshes).
12. **Click through:** a cell opens its full view (master map, Model Viewer, the drought window) at the sheet's view and time.
13. **A text cell** (the forecast discussion's first lines, a number: the day's maximum) and a **table / point graph** cell: not maps; their size and font follow the sheet.
14. **Saved sheets:** the template as a file (like the Checks presets): the layout, each cell's kind and layers; the view is not part of it unless asked (a "same view" sheet for a repeating event).
15. **Not a layout editor (the edge of the idea):** no free placement, no drawing, no layers of annotation; a template picks from a few layouts and the cells fill them. The sheet is not Photoshop.

The cases above are accepted as the sheet's requirements (the user: "everything here I think I'd suggest"), with 8 and 11 as marked.

### What the panels need to be (see section 27's notes)

A cell must be drawable **without a window**: a model chart is (`GfsRender::png`, a size and a view), the drought maps are (paint to a size), pictures are; the master map's layers are not yet (a headless `MapHost`, and a signal for "this layer has finished loading", are missing).

## 29. The flat pictures of the drought and climate windows, to zoomable layers (sketch, 2026-11-18)

Asked: "any climate cleanup, raster PNG to zoomable?" The drought window's Normals tab and the CPC soil moisture percentile are zoomable (section 27); these are not yet. Sources found on 2026-11-18, in the order they would be done.

| picture now | zoomable source | needs |
|---|---|---|
| **Drought outlook, this month and the season** (CPC `mdohomeweb.png`, `sdohomeweb.png`) | `https://ftp.cpc.ncep.noaa.gov/GIS/droughtlook/mdo_polygons_latest.zip` and `sdo_polygons_latest.zip` (17 MB each; dated copies `mdo_polygons_YYYYMMDD.zip`, `sdo_polygons_YYYYMMDD.zip`, and `sdo_update_polygons_YYYYMMDD.zip` for the seasonal update). Inside: `DO_Merge_Clip.shp` (12.8 MB, polygons, field `Outlook`: No_Drought, Persistence, Improvement, Removal, Development, Worsens; `Fcst_Date`, `Target` "Nov 2026") and **`MDO.kmz` / `SDO.kmz` (3.9 MB; the same polygons, one placemark for each category)** | a KMZ reader that keeps the placemark's name (the Monitor's `parseKmz` keeps only a D-number), and a zone layer in `DroughtMap` (named colours, a key, the name under the pointer). No new library: the Monitor's KMZ reader and `DroughtMap` do the rest. Issued on the last day of each month (the season's on the third Thursday as an update); the file name changes, `..._latest.zip` does not: keep it by `Fcst_Date`. |
| **Soil moisture rank, daily** (leaky bucket GIF) | not found: `Soilmst_Monitoring/Figures/daily/curr.w.rank.daily.gif` has no grid beside it that was found in `ftp.cpc.ncep.noaa.gov/GIS` | find the CPC leaky bucket's grid (the `GIS/USDM_Products/soil/percentile` 1 degree grids are the nearest, the same family as the Normals tab's) |
| **Standardized precipitation index, 3, 6, 12, 24 months** (GIFs) | no SPI grid in `GIS`. `GIS/USDM_Products/precip/percentile` is the precipitation percentile (1 degree) | either that grid, or SPI computed from the ACIS / MRMS totals the Normals tab already reads (a gamma fit by place and month: new code) |
| **EDDI** (new, not a conversion) | `GIS/USDM_Products/EDDI/EDDI_Weekly_TIFFS.zip`, `EDDI_Monthly_TIFFS.zip` and the `-change_` pairs (0.6 MB each, updated every Thursday) | the zip reader and `loadSoil`'s GeoTIFF path: a new entry in the Outlooks list |
| **Climate window: sea surface temperature** (OSPO PNG / GIF) | OSPO CoralTemp NetCDF `https://www.star.nesdis.noaa.gov/pub/sod/mecb/crw/data/5km/v3.1_op/nc/v1.0/daily/sst/YYYY/coraltemp_v3.1_YYYYMMDD.nc` (answers 200 for 2026-11-16) | GDAL NetCDF read (already used for other layers), a 5 km global grid: a master-map raster layer, not a window |
| **Climate window: ocean currents** (ocean.weather.gov GRTOFS / NCOM PNGs) | NOAA RTOFS on AWS (`noaa-nws-rtofs-pds`, `rtofs.YYYYMMDD/`) | big NetCDF on a tripolar grid: later, as particles on the master map |

**Order:** the drought outlooks (built next, 2026-11-18), then EDDI (small), then the SPI question, then the climate window's layers with the master map's.

**Built (2026-11-18):** (1) the **drought outlooks**: `UtilityDrought::parseOutlookZip` (the KMZ inside the CPC zip, a zone for each outcome by the file's own placemark names) and `DroughtMap::setZones` (named, coloured areas; the contiguous states; the outcomes under the pointer); `DroughtViewer::loadOutlookZones` reads the newest file out (the monthly `mdo_polygons_<last day of the month>`; the seasonal one is posted on the third Thursday as `sdo_polygons_<date>` and updated on the last day as `sdo_update_polygons_<date>`, so both names are tried, newest first), 17 MB each, kept for good. Seen: the monthly outlook of 31 Oct (valid November) and the seasonal update (valid to 31 January, with a development and a worsening area in the Northwest). (2) **EDDI**: five entries on the Outlooks tab (1, 4 and 12 weeks; 3 and 12 months) read from `EDDI_Weekly_TIFFS.zip` and `EDDI_Monthly_TIFFS.zip` (0.6 MB each, every file 464 x 224 cells at 1/8 degree over -125 to -67, 24 to 53, Float64, standard units from about -2.2 to 2.2, `ValidTime` in the metadata) through `loadSoil` (an `eddi:<zip>|<file>` address); red where the demand is above normal, blue below. **Left:** the weekly and monthly "change" zips (`EDDI-change_Weekly_TIFFS.zip`; the monthly one is not in the listing), the soil moisture rank and SPI grids (not found), the climate window's layers.

**GDAL kept at the newest release (2026-11-18, the user: "push the point of updating to the very most recent GDAL on GitHub releases"):** the packages had GDAL 3.12.4 (vcpkg's port stops there) while the project's newest release was **3.13.3** (18 Aug 2026; 3.13.4 is not out: no release or tag). (1) An **overlay port** `.github/vcpkg/ports/gdal` (vcpkg's own `ports/gdal` files, its seven patches checked to apply in order to the 3.13.3 source with `patch -p1`, the version and the SHA512 of `archive/v3.13.3.tar.gz` changed), named by `--overlay-ports` in both vcpkg installs of `build.yml` and part of both cache keys; the Windows smoke test now fails when the bundled GDAL is not the version the port names. **Not yet tried in CI** (a Windows rebuild of GDAL is about 20 minutes; the first build may need a patch fixed). This machine runs 3.13.3 (Arch) and the program works on it. (2) **The check**: `catalog-check.yml` (the daily sources workflow) asks GitHub for the newest OSGeo/gdal release, compares it with the port's version, writes it in the run summary and opens, updates or closes an issue labelled `gdal-update` when behind. (GDAL has no official update check of its own; its GitHub releases are the project's list.) (3) **Wording:** Settings > GDAL, "Check for a newer GDAL", says why and how to update when a newer release is out, and `docs/help-gdal.md` says the same.

**The nice and gross day combined (2026-11-18, the user: "let's combine the nice and gross day: add the features of nice and just include the rules for gross"; "the day boxes should have the highest day, minus the days with dangerous grosses ... same as the SPC outlook page"):** the Nice weather pages (the day pages, the days 1-8 overview, the home picture) now draw `nice_gross_weather` and not `nice_weather` (`NiceWeatherCache`), so a day is one map with the nice levels (hover: what limits the level, the ingredient scores, the caps) and the gross side where there is one (-1 Gross, -2 Nasty, -3 Miserable); Decent is shown (light green) as the nice page had it, no longer blank "meh" (the answer to "Decent?": show it). The summary beside the map counts the gross tiers and the regions' averages count them as -1 to -3; the headline adds "Gross or worse N%" from 1%. **The overview's boxes are the SPC summary's** (`SpcSwoSummary`: the highest risk of the day on its colour): each box says the highest nice level that covers at least 1% of the area, on that level's colour (Decent to Perfect); a day with **dangerous gross weather** (Miserable over 1% of the area, or Nasty or worse over 10%; `NiceWeatherCache::Shares::dangerous`) says "Dangerous Miserable" or "Dangerous Nasty" on the gross colour instead. Seen on 18 Nov: day 1 Beautiful, days 2 to 7 Nice, day 8 Fair, none dangerous (so the dangerous box is **not seen**; the thresholds are guesses to be set by eye). Also: the Nice overview no longer shows the activity line twice in the Outlook screen (as the day page). `WXQT_OPEN=niceoverview` shows the overview (the eight days are made one after another: about 7 minutes headless). Catalogue regenerated (the chart's title changed); the alias and Gross tests pass.

**The SPC convective outlook on the master map, days 1-8 at their valid time, and the layer panel's width (2026-11-18, the user: "I would like to be able to add the SPC outlook (show each Day 1-8 at the valid time)"; "I'd like first to be able to resize the sidebar ... can't see the end of the slider"):** (1) a layer `severe/spc-outlook` (`SpcOutlookLayer`): the SPC's own GeoJSON of the areas (`products/outlook/day{1,2}otlk_{cat,torn,wind,hail}.nolyr.geojson`, day 3 `_cat` or `_prob`, days 4-8 `products/exper/day4-8/day{4..8}prob.nolyr.geojson`), drawn in the SPC's fill and stroke colours, holes kept; time-aware: the time bar's eight frames are the 12Z starts of days 1 to 8 (the start is read from the day 1-3 files' `EXPIRE_ISO`), each shows the day that holds the time (before day 1 or after day 8: nothing; live: day 1); hover says the risk, the period and when it was issued; option: what days 1 and 2 show (categorical, tornado, wind, hail; day 3 shows the total severe probability for the last three, days 4-8 their 15% and 30% areas); `setAsOf` follows the history page's time. Seen: day 1 and day 2 of 18-19 Nov (thunderstorm and marginal areas), with MRMS under it. **Not seen:** a day with a slight risk or more, days 3 to 8 on screen, the tornado/wind/hail choices, the loop. Historic outlooks (the archive) are not read: only the current ones. (2) **The layer panel:** a thin bar between the map and the panel is dragged to resize it (260 to 900 px, kept as `MASTERMAP_PANEL_WIDTH`), the manager scrolls sideways when a control is wider than the panel (the end of a slider is reachable) and a long entry in an option's combo box is cut in the box (`AdjustToMinimumContentsLengthWithIcon`) instead of widening the panel. **Not seen with the options open** (the headless route did not open them).

**Dependencies kept current (2026-11-18, the user: "any other stale dependencies? ... do all"; "check outside sources for an RSS feed for libtiff / SQLite"):** checked against the newest releases (major.minor.patch): GDAL 3.12.4 against 3.13.3 (the overlay port, above); **Qt 6.4.2 in the Linux AppImage** (Ubuntu's package) and 6.8.3 on Windows against 6.12.x; netcdf-c 4.9.3 against 4.10.1; the catalogue check's GDAL (Ubuntu's gdal-bin, the 3.8 series); everything else vcpkg carries (PROJ 9.9.0, GEOS 3.15.0, HDF5 2.2.0, curl 8.23.0, OpenJPEG, libaec, libgeotiff, libtiff 4.7.2, SQLite 3.54.1 ...) is current; OpenSSL stays on vcpkg's 3.x (upstream is 4.1) and libpng on 1.6 (1.7 is a beta). **Done, not yet run in CI:** (1) **Qt 6.10.3 through aqtinstall in both packages' jobs and in the catalogue check** (`install-qt-action`; Linux no longer uses `qt6-base-dev`; `makeAll.py --pro --qt5` for the plain `qmake`; the AppImage script takes the plugins from `QT_PLUGIN_PATH`/`QT_ROOT_DIR`; the xcb libraries the system must give are installed): 6.10.3 because it is the newest aqtinstall 3.3 can fetch **for Windows** (6.11 and 6.12 are in a repository layout it does not read: Linux has 6.12.1); the code already builds on 6.12 (this machine) and 6.4 (the old Linux job). (2) **The catalogue check's GDAL** is the packages' vcpkg build (same cache key and build as `build.yml`'s Linux job; the tools reached through small wrapper scripts that put the libraries on the library path for one run only). (3) `actions/cache` v5 to **v6** (6.0 is ESM and package updates, no input changes). **Not done:** the **netcdf-c overlay**: vcpkg's patches fail against 4.10.1 (`no-install-deps`, `dependencies`' plugin hunk, `backport-d7895f6`: the install-dependencies and plugin linking code of the CMake build changed); porting them needs a trial build of the NetCDF driver and the plugins on both systems: wait for vcpkg's own port, which the daily check will show. (4) **The daily check** (`tools/deps_check.py`, a step of `catalog-check.yml`) now compares all of these with their newest releases and opens one issue labelled `deps-update` while any is a major or minor version behind (today: Qt, netcdf-c); patch-only differences go to the run's summary. **Feeds:** libtiff has none on GitHub: GitLab's atom feed of its tags (`https://gitlab.com/libtiff/libtiff/-/tags?format=atom`; also `.../-/releases.atom`; the files are on `download.osgeo.org/libtiff/`); SQLite has no feed of its own for releases: its GitHub mirror's tags (`https://github.com/sqlite/sqlite/tags.atom`, titles "version-3.54.1: Version 3.54.1") and the project's timeline (`https://sqlite.org/src/timeline.rss`, every check-in). Both are read by the check.

**Qt from vcpkg (2026-11-18, the user: "so let's do qtbase probably (more work now, but future gains?)", after finding that Qt 6.10 has had its last patch, that aqtinstall 3.3.0 (June 2025) cannot read Qt 6.11+'s Windows repository and that its fix, merged 24 Mar 2026 as #1000, is unreleased):** a **trial** that does not touch the packages' workflow: `.github/vcpkg-qt/vcpkg.json` (qtbase 6.11.3 today, vcpkg's version: only gui, widgets, network, concurrent and what they load; windeployqt on Windows; on Linux dbus, fontconfig, ICU, OpenSSL and the xcb libraries) and `.github/workflows/qt-vcpkg-trial.yml` (run by hand): Windows builds Qt with vcpkg (cached in `D:/qt-vcpkg`; the first build takes an hour or more), puts `tools/Qt6/bin` (qmake, moc, rcc, uic, windeployqt) and `bin` on the PATH, runs `qmake -query`, builds wxqt with jom, runs `windeployqt` and starts the result. **Not run yet.** Known unknowns: vcpkg's qmake is a wrapper over `tools/Qt6/qt_release.conf` made for qmake projects (the layout of the headers and libraries is vcpkg's: whether `wxqt.pro` builds with it is the first thing to see); where the plugins are (`Qt6/plugins`) and whether `windeployqt` finds them; the cost: the vcpkg Qt is one minor behind (6.11.3 against 6.12.1 while vcpkg's port lags). **Linux next**, only after Windows is green: vcpkg builds the xcb and X11 libraries from source and asks for the system's development packages (vcpkg prints the list); until then Linux stays on the official binaries (aqtinstall reads Linux's 6.12.1, so the Linux job can move from 6.10.3 to 6.12.1 at once). When both are green: `build.yml` and `catalog-check.yml` take Qt from this manifest and `install-qt-action` goes.

**Qt from vcpkg: the Windows trial is green (2026-11-18, run 44355303702):** qtbase 6.11.3 (vcpkg's port) built in 34 minutes with the release triplet as host, the install tree (190 MB) saved at once, `qmake -query` and `qmake` built `wxqt.pro` with `jom` in 20 minutes (no sccache), `windeployqt` made a 106 MB folder (the generic, imageformats, networkinformation, platforms, styles and tls plugins, `vc_redist.x64.exe`) in 13 seconds and `wxqt.exe` started and stayed up for 12 seconds. The first trial's 83 minutes were the host triplet building Qt twice (debug and release). **`build.yml`'s Windows job now takes Qt this way** (`aqtinstall` is no longer used on Windows); Linux stays on the official binaries, moved from 6.10.3 to 6.12.1 (6.10's last patch release was 6.10.3; aqtinstall reads Linux's 6.12). Not yet run in the packages' workflow. **Left:** a Linux trial of vcpkg's Qt (X11 libraries from source), after which `install-qt-action` goes from both Linux workflows too.

**The Nice day reads 6 hourly hours only (2026-11-18, the user: "six hour only for now"):** the blend's 3 hourly hours to 36 are no longer read (`sampleHours`; `WXQT_NICE_3H` brings them back for a test). Against his levels at all of his cities, on 11 cases (his updates of 16, 17 and 18 Nov, days 1 and 2, our 06Z and 12Z runs): the same level in +0.75 points more of the cities on average (range -1.2 to +2.8; five clearly up, two down), within one level +0.04 (range -0.3 to +1.2), 2 to 8% of the cells change level; days 3 to 8 are unchanged (their hours were 6 hourly already). Download from a cold cache: day 1 69 MB (127 before), the day 2 that follows 51 MB. The marks were fitted with 3 hourly hours; the daily check (`yall-nice/check.py`) will show any drift. Earlier cuts of the same day (2026-11-18): the blend's temperature and humidity fields no longer fetched (3.4 MB an hour), GEOS-FP asked for as the United States only (0.10 MB a step) and only beyond blend hour 72 (the RRFS has the smoke before it).

**Dependencies behind (issue 5 of the daily check, 2026-11-18; not an immediate need, kept here so the closed issue is not the only record):** (1) **netcdf-c** is built with 4.9.3 (vcpkg's port); the newest release is 4.10.1. vcpkg's patches fail against it (`no-install-deps.patch`, the plugin hunk of `dependencies.diff`, `backport-d7895f6.diff`: the CMake build's install-dependencies and plugin-linking code changed). Either wait for vcpkg's own port (`microsoft/vcpkg` ports/netcdf-c is still 4.9.3#5 on 2026-11-18) or port the patches as an overlay port in `.github/vcpkg/ports/netcdf-c` (as GDAL's) and try the NetCDF driver and the plugins on both systems. It matters for the models read by byte range from NetCDF (SPC mesoanalysis, GEOS-FP, the climatology). (2) **zlib** is built with 1.3.2 (the vcpkg checkout of the runner image), the newest is 1.4 (released 5 Nov 2026; `microsoft/vcpkg` has had it since 13 Nov, with the fixes for CVE-2026-76844 and CVE-2026-85091 of 4 Nov): the runner's checkout is older than vcpkg's master. Ways: an overlay port of master's `ports/zlib` (as GDAL's; every library that uses zlib is then built again, once), or a vcpkg checkout of our own at a chosen commit in place of the image's (`builtin-baseline`, or a fetch of one commit): that moves every port at once, which also fixes the next lag like this, but needs a try of the whole set. (3) The issue's third line, **Qt**, is out of date: Windows is vcpkg's 6.11.3 and Linux 6.11.3 now (0.2.0), the newest is 6.12.x. The daily check opens a new issue when it next finds something behind (issue 5 is closed).


## 30. Pending and done since 2026-11-18 (the user: "Plan.md ... I know we had a few pending"; written 2026-11-19)

**Done and pushed (b8c5e93, the commits before fb4b39c are on main):** the home screen starts in stages (severe dashboard, then the downloaded pictures in the home order, MRMS, the model charts, the Nice day last; each after the one before or 25 s); the master map's Export has "The model chart at full resolution, whole world or polar"; the Nice day 1 is drawn from the date's 12Z run once the newest run is later than 12Z (a late run had nothing for the places whose daytime was past: the line at the Mississippi); the Network window's "Of" for a byte range is its length; the **AirNow** layer on the master map and a home thumbnail (off by default); the **aurora** (SWPC) and **Drought Monitor** home thumbnails from their prerendered pictures; the hourly graph takes the column's width and, from 1180 px, the forecast point card stands beside it ("Gen Tstrm" when the words do not fit); the drought "Change since" works again (the KMZ title lost its colon after "Valid"); the Daily tab's 1991-2020 normals are made a month at a time (30 files), kept, and the source files not kept; the kept normals are read again (the key "name|code" miscounted fields, so they had been made on every run); Qt 6.11 cleanup.
**Done, committed here, not pushed:** (fb4b39c) **Qt 6.8 or newer**: the 6.4 to 6.7 fallbacks and QtCompat.h removed, deprecated APIs before 6.8 are an error (`QT_DISABLE_DEPRECATED_BEFORE=0x060800`), the project refuses an older Qt, README_OS has trixie in place of bookworm; (ba816d4) `makeAll.py --cache` (sccache or ccache when installed, a note when not).

**Pending, in the order they matter:**
1. **The 0.2.1 packages run (44391854540)** was started after the Windows DLL fix (fd9c561) and not looked at: the step "Package portable folder" must print the libraries copied from Qt's bin and pass the closure check, and "Smoke-test the portable folder starts (clean PATH)" must show wxqt.exe running after 12 s. New PowerShell, never run on Windows. Then: publish 0.2.1, and edit the 0.2.0 release notes (its Windows files lack libpng16, freetype, harfbuzz, md4c, double-conversion, pcre2-16, zstd, z, brotli, bz2: the AppImage is fine). Not done without the user's word.
2. **zlib 1.4 and netcdf-c 4.10.1 overlays** ("Let's do overlays for both", paused for the DLL problem): `.github/vcpkg/ports/zlib` (master's 1.4 port, verified) and `.../netcdf-c` (still 4.9.3's files; regenerated patches in the scratchpad: no-install-deps.patch, dependencies.diff; drop backport-d7895f6.diff and the three Windows stat patches; REF v4.10.1, SHA512 6be6fee5...; vcpkg.json 4.10.1) are untracked; then the zlib hash into the Qt and webp cache keys and `--overlay-ports` for both, and a CI trial (long: Qt, GDAL and webp rebuild).
3. **Help pages** (docs/help-todo.md, docs-local): Nice weather and Gross days first, the chart of the day, the master map's SPC and AirNow layers and the layer panel, drought outlooks / EDDI / the Daily tab, the Model Viewer's preload, the HREF, NBM and GEOS-FP pages, the home thumbnails (AirNow, aurora, drought monitor, polar vortex, chart of the day).
4. **Home screen, small:** the click targets of the aurora ("goes16.png" space weather) and drought ("fire_outlook.png#2") tiles are the toolbar ids, not clicked in the app; the right-click save of a home tile names the main window ("wx update every 10 min ...") in its footer, not the tile, and the polar tiles save the 640 px picture (the full resolution one is the master map's Export); the forecast point card's "Excessive rainfall" row looks clipped against the weekly table; the card is tight at 1180 px (the day columns touch). Not yet done: the small text items (current conditions, 7 day, hazards) are not in the stages. His idea, noted: more tiles (Nice days 2-3 or the 8 day strip from the cache, SPC day 2 fire and the day 1 probabilities, the drought outlook, a space line of Kp and aurora, an active tropical storms line). WPC 5 day and fronts / day 2 tiles: no (he has them on the phone / in the severe dashboard).
5. **MPAS-RN** reads its tar header by header (a 512 byte range 4.37 GB into the 12Z tar, "on screen" priority): look at how the member is found and whether the start-up reads can be lighter.
6. **Nice day 1:** drawn from the 12Z run after 12Z (his own is a morning snapshot too, updated 11:31 AM CST = 1731z); a fresher version (the hours already past from the 12Z run, the rest from the newest) costs a second run's downloads and was not wanted for matching him.
7. **Linux vcpkg Qt trial** (after Windows proved): Linux still uses aqtinstall Qt 6.11.3; aqtinstall cannot read Qt 6.12's checksums on release day (it is fixed in aqtinstall master, PR 1000, unreleased).
8. **Cleanups waiting:** README_OS.md still has the Qt 5 sections of old distros; CI could use `makeAll.py --cache` in place of its own `make CXX="sccache g++"` (left as it is); the dev aids `WXQT_OPEN=homeshot:<w>:<h>:<out.png>` and `WXQT_GRAB=<out.png>` (the drought window) are in main.cpp.
9. **Dependencies the daily check reports behind** (see the section above): netcdf-c and zlib (2 above); libtiff and SQLite are watched through their tag feeds; OpenSSL is held.

**RRFS smoke against AirNow, 19 Nov 2026 (the user: "Compare AirNow to RRFS?"; section 24's 'a smoky day still to try' is not this: it was a quiet day):** `pm25_sfc` of the 2026111900 run at F015 (15Z, 9 o'clock CST) at the 576 reporting areas with a PM2.5 observation today (AirNow's `reportingarea.dat`, the newest 'O' row of each; the chart read by `WXQT_SAMPLES`). AirNow's PM2.5 index: mean 36, median 36, 178 areas over 50. The RRFS's PM2.5 (smoke and dust only): mean 0.8, median 0.1 micrograms, 6 areas over 9 (the Moderate line). Correlation 0.14, as the model has no traffic, industry or wood stoves: the day's AirNow is the background of the cities (the Dallas-Fort Worth 130 and Indianapolis 114 are 0.8 and 1.5 in the RRFS). Where the RRFS has a plume it agrees in kind: Libby MT 28.9 micrograms (AirNow index 71), Clarkston WA 24.8 (6), Lewiston ID 24.8 (37), Lapwai ID 23.6 (58), Ritzville WA 10.1 (51), Coeur d'Alene ID 9.3 (52): five of the six are AirNow Moderate or near it, Clarkston is not. One time, one day, not a verification.
