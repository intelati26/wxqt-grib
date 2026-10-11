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
| `kind` | `grib` (an index beside each file), `whole` (one field per file; the members are its messages), `netcdf` (variables read by range) |
| `url` | the place of a file, repeatable (more are other places, tried in order). Tokens: `{date}` `{cycle}` `{prev_cycle}` `{hour}` `{hour2}` `{hour3}` `{file}` `{dir}` `{name}` |
| `file` | which file holds a field: `when <conditions> => template`, the first that matches; a line without `when` is the default. Conditions: `variable=A,B`, `level="2 m above ground"`, `level~"regex"`, `stat=` (none), `stat~regex`. Template tokens: `{variable}` `{level}` `{stat}` `{stat|default}` `{mb}` `{mb4}` |
| `file_default` | what `{file}` is when a want has none (the run check, the first file) |
| `index_format`, `index_suffix`, `index_replaces`, `default_detail` | `noaa` (`.idx`) or `ecmwf` (JSON); the index's name; what record detail to ask for |
| `member` | members of a `whole` file as bands: `c00:1, p01-p20:2-21` |
| `value_scale` | `VARIABLE:factor, ...` units |
| `warp` | `step,west,south,east,north` for a grid that is not latitude / longitude |
| `extra_missing` | a value that means "no data" besides GRIB's |
| `cycle_hours`, `lag_hours`, `probe_file`, `probe_hour`, `cycles_to_try`, `max_parallel` | how runs are looked for |
| `hours` | `0-192/3, 198-384/6` |
| `run_every`, `hourly_runs`, `on_missing`, `preload_hours`, `run_length` | the run list and what to do when a run lacks a field |
| `sectors`, `overlays`, `list` | `world` or `conus`; lines and barbs can be ticked on; the charts are the registry's |
| `clone`, `clone_ids`, `clone_variables`, `clone_levels`, `clone_fields`, `clone_skip`, `clone_plain`, `clone_rename`, `clone_prefix`, `clone_pieces`, `clone_specific_humidity` | draw the GFS charts from this model's fields: only those whose variables (and levels, `TMP\|850 mb`) the model has |

A model that needs its own charts (a spread, a probability) needs a recipe in `GfsChart.cpp`; the clone gives the standard map set.

`tests/gfs/run_modelfile.sh` checks that the files describe the models they replaced, byte for byte (urls, file names, hours, clone rules).
