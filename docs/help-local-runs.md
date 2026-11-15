# Local model runs

Settings > **Local runs** is for models you run yourself, on this computer. The Model Viewer lists their runs with the others,
and they can be compared with any model, or with the MRMS radar, at the same valid time.

## What a run is

A **folder of WRF-format history files** (`wrfout_d01_...`), one folder for each run. The grid can be any WRF grid (Lambert, a
rotated lat-lon ...): the program places it on latitude and longitude from the files.

## Telling the program where they are

"Folders (globs allowed)" takes one or more places, separated by `;` (a `*` stands for any name): for example
`~/wrf/runs/*;/data/arwen/*`. Empty is the places the program looks by itself. **Add a folder...** picks one with a dialog;
**Use the default** empties the list. The table below shows the runs found, with their hours; "No runs found in these folders"
means the pattern matched nothing, usually a missing `*`.

A run is listed by the time of its first file. A model file (`kind = wrf`, `run_folders`) can name the folders for one model
instead: see *Model files*.
