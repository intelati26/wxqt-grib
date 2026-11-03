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
