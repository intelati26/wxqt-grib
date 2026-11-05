# Model definition files

A model the Model Viewer draws can be a text file instead of code. Built-in models that fit are written this way
(`src/gfs/GfsBuiltInModels.cpp` is the readable set of examples); your own go in the folder

    <app data>/models/   (Linux: ~/.local/share/<org>/<app>/models)

as `name.model`. They load when the program starts and appear in the model picker and in Settings > Model Sources. A file that
does not parse is left out, and the line is reported in that tab's problem list.

Lines are `key = value`; `#` at the start of a line is a comment. Values with spaces at the ends go in quotes.

| key | meaning |
|---|---|
| `id`, `label` | the name in the picker, and the credit under a chart |
| `source_label` | the credit when it differs from `label` |
| `group` | the picker's group for it |
| `kind` | `grib` (an index beside each file), `whole` (one field per file; the members are its messages), `netcdf` (variables read by range), `om` (Open-Meteo's `.om` files on their open bucket: one file an hour holding every variable, read by range; the variable's name is the `nc_variable` rule's), `wrf` (WRF-format runs on this computer: ArWen, WOOF-NAM; `run_folders`), `custom` (read by code registered for the id: SPCREFS) |
| `url` | the place of a file, repeatable (more are other places, tried in order). Tokens: `{date}` `{year}` `{month}` `{day}` `{cycle}` `{prev_cycle}` `{hour}` `{hour2}` `{hour3}` `{file}` `{/file}` (a slash and the file, or nothing) `{file3}` (its first three letters) `{dir}` `{name}` (the file split at `/`) `{tag}` `{tail}` (split at `:`) `{storm}`. `url = when file~"-s$" => ...` is chosen by the file's name: the first conditional url that matches is the place; with none, the plain ones. Several plain urls are places to try in order |
| `subset_url` | a server's box request: tokens as above plus `{variables}` `{levels}` (made from `subset_variable = &var_{v}=on` and `subset_level = &lev_{l}=on`) and `{west}` `{east}` `{north}` `{south}` |
| `file` | which file holds a field: `when <conditions> => template`, the first that matches; a line without `when` is the default. Conditions: `variable=A,B`, `level="2 m above ground"`, `level~"regex"`, `stat=` (none), `stat~regex`. Template tokens: `{variable}` `{level}` `{stat}` `{stat|default}` `{mb}` `{mb4}` |
| `file_default` | what `{file}` is when a want has none (the run check, the first file) |
| `index_format`, `index_suffix`, `index_replaces`, `default_detail` | `noaa` (`.idx`) or `ecmwf` (JSON); the index's name; what record detail to ask for |
| `member` | members of a `whole` file as bands: `c00:1, p01-p20:2-21` |
| `value_scale`, `value_offset` | `VARIABLE:factor, ...` and `VARIABLE:amount, ...`: units (meters to millimeters; Kelvin to degrees C is `TMP:-273.15`) |
| `nc_variable` | a NetCDF file's name for a want, rules as `file`: `nc_variable = when variable=HGT level~"^[0-9]+ mb$" => H{mb}`. For `kind = om` it is Open-Meteo's name; `speed/direction` (`wind_speed_10m/wind_direction_10m`) on a UGRD or VGRD rule makes the component from a wind given as speed and the direction it comes from (m/s, degrees) |
| `valid_offset_file` | `FILE:minutes`: a collection (`{file}`) stamped differently from the rest (GEOS-FP's 3 hour aerosol averages are stamped in the middle of their period, `tavg3_2d_aer_Nx:-90`) |
| `valid_offset` | minutes added to the run's time plus the hour for the tokens `{valid}` (`yyyyMMdd_HHmm`), `{valid_date}`, `{valid_hour2}`, `{valid_minute2}`: a time average is stamped half an hour after it starts |
| `run_folders` | for `kind = wrf`: the folders of WRF-format runs on this computer (ArWen, WOOF-NAM, WRF), globs separated by `;`, `~` is the home folder. A run is a folder of hourly `wrfout_d01_<yyyy-mm-dd_HH:MM:SS>` files (NetCDF), its time the first file's. The grid (Lambert, rotated lat-lon, any) is put on latitude / longitude from `XLAT` / `XLONG`. `nc_variable` gives WRF's names (`T2`, `PSFC`, `RAINNC+RAINC`); `earth(U10,V10)` turns grid-relative winds east / north with `SINALPHA` / `COSALPHA`; `max(REFL_10CM)` is the highest of the levels |
| `om_grid` | for `kind = om`: `regular` (the file's latitude / longitude grid, its box from the meta.json beside it; square cells only) or `gaussian 320 0.25` (ECMWF's octahedral reduced Gaussian grid O320 / O1280, interpolated to that many degrees). Lambert and rotated grids are not read yet. The url's `{valid_iso}` is the file's `2026-11-09T0600`; a run's folder is `{year}/{month}/{day}/{cycle}00Z/` |
| `warp` | `step,west,south,east,north` for a grid that is not latitude / longitude |
| `extra_missing` | a value that means "no data" besides GRIB's |
| `cycle_hours`, `lag_hours`, `probe_file`, `probe_hour`, `cycles_to_try`, `max_parallel` | how runs are looked for |
| `hours` | `0-192/3, 198-384/6` |
| `run_every`, `hourly_runs`, `on_missing`, `preload_hours`, `run_length` | the run list and what to do when a run lacks a field; `run_length` is `every 6 : 84 , else : 18` or by cycle, `0:240, 12:120, else:30` |
| `sectors`, `overlays`, `list` | `world`, `conus` or `storm`; lines and barbs can be ticked on; the charts are the registry's |
| `storm` | the model is run for one storm at a time: its grid is the storm's and its source is named `ID-<storm>` |
| `clone`, `clone_ids`, `clone_variables`, `clone_levels`, `clone_fields`, `clone_skip`, `clone_plain`, `clone_rename`, `clone_prefix`, `clone_pieces`, `clone_specific_humidity` | draw the GFS charts from this model's fields: only those whose variables (and levels, `TMP\|850 mb`) the model has |

A model that needs its own charts (a spread, a probability) needs a recipe in `GfsChart.cpp`; the clone gives the standard map set.

All 19 built-in models are written this way. `tests/gfs/run_modelfile.sh` checks that the files describe the models the code had been, byte for byte (urls, file names, hours, clone rules, a storm model's urls, the GFS's box request).

## Edge cases the format had to take in

- **File names that depend on the want**: the GEFS's three sets of files (`file = when variable=GUST,... => {stat}-s`), the REFS's three places (`url = when file~...`), the AI model's pressure and surface files.
- **A url that depends on the file**: the GEFS names a file after the first three letters of the file token (`ge{file3}`), the REFS has one place for the deterministic run, one for the products and one for members.
- **A default file**: the run check asks for no particular field (`file_default`).
- **Alternatives and globs**: SPC's guidance names a file by the GEFS cycle it was made with, or the one before (`{ee}`, `{pe}`, a `;` between alternatives), and by an accumulation that varies (`*` in the name).
- **A place that is in the test area until it is in the real one**: two plain `url` lines.
- **Hour 0 and hour-less files**: `{hour3}` is simply left out of the url (the mesoanalysis has one file an hour).
- **A model that follows a storm**: `{storm}` in the url and in the source's name.
- **Units**: `value_scale = APCP:1000` (meters to millimeters).
- **A server found by the user** (NASA GEOS-FP, NetCDF-4): the time in the file name is the run plus the hour plus half an hour (`{valid}`, `valid_offset`); the file names its fields its own way (`nc_variable`); temperatures are Kelvin (`value_offset`); the aerosol collection (`file = when level="aerosol" => tavg3_2d_aer_Nx`) is 3 hour averages stamped in their middle (`valid_offset_file`); the grid is 0.3125 by 0.25 degrees, which the charts' square-cell grids cannot hold, so it is warped to 0.25 (`warp`); the 00Z run is 10 days, the 12Z 5, the others 30 hours (`run_length` by cycle); a run takes about eight hours to appear (`lag_hours`); and below-ground points in pressure levels are fill values, drawn as holes.
- **What stays code**: the readers (the Zarr store, the NetCDF range reads, GDAL warping) and the models' own charts (`GfsChart.cpp`).

## Open-Meteo's `.om` files (`kind = om`)

Open-Meteo keeps the output of many weather services on an open bucket (`openmeteo.s3.amazonaws.com/data_spatial/<model>/<year>/<month>/<day>/<cycle>00Z/`) in its own file format
(open-meteo/om-file-format): one file for each forecast hour holding every variable, plus a `meta.json` for the run. `OmFile` (`src/gfs/OmFile.cpp`, with the vendored decoder in
`src/external/omfile/`) reads a file by byte ranges: a variable of a 100 to 170 MB file costs a few megabytes. The model's definition names the folder in `url` (`{valid_iso}` is the file's
`2026-11-09T0600`), the variables in `nc_variable` rules and the grid in `om_grid`; a run is found by looking for the hour-0 file (`probe_hour`; the Hybrid GEFS has none, so 6), not by reading the
`latest.json`, which for the EC46 was found a week old. The rest (the cache, the charts cloned from the GFS ones, the model picker) is as for any model.

- Values are in the units Open-Meteo stores (degrees C, hPa, percent, m/s, mm): `value_scale = PRMSL:100` gives the pressure in Pa as the charts have it. JMA's GSM heights are the height divided by
  9.80665 (`HGT:9.80665`). Check a new model against Open-Meteo's own API at a point before trusting it.
- `om_grid = regular`: the file's latitude / longitude grid, its box from the `meta.json` (south row first in the file, flipped here). Cells that are not square (the UK Met Office's) are resampled
  onto the finer step. `om_grid = gaussian 320 0.25`: ECMWF's octahedral reduced Gaussian grid (EC46 is O320), interpolated to that many degrees (`OmGrid`). Rotated and Lambert grids are not read yet.
- A model with its pressure levels in a second folder (the GDPS) has a url `when file~"^upper$" => ...` and a `file = when level~"^[0-9]+ mb$" => upper` rule.
- A model with surface fields only sets `clone_levels = 1 mb` (no level of the GFS charts matches) and lists the variables it has.
- The EC46 weekly map reads these files (`Ec46Loader`), the API only when they have no such run, week or field; the plume at a point stays on the API.

## Help pages

The words about a built-in model are a Markdown page, `help/models/<id>.md`, bundled by `help/help.qrc` and shown by the Help window (`setMarkdown`; the program adds the charts, hours and
runs after it). A new model needs its page, its line in `GfsModels.cpp` and, if it is not in a group by its `group =` line, a place in `pickerGroup`. A model added as a `.model` file keeps `why` and
`link` lines instead.
