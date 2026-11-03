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

## 5. Open-Meteo's `.om` files on S3: grids with no API calls (for Thursday)

**Why:** one EC46 map through the API is some 800 requests (up to about 5,000 by the service's weighting) of the 10,000 a day. Open-Meteo also publishes the forecast grids as files in a public bucket, read by plain HTTPS range requests, which do not count against the API allowance.

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
