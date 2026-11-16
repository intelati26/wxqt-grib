# Editing the program's data yourself: the rules

Four things the program reads are plain text files you can change yourself, without waiting for a new version of the program: **where the data comes from**
(servers), **what a model's files are** (model files) and **which maps there are** (chart files). This page is the rules they
share and which to touch for which job. The details of each format are in its own file:

| what | file | details |
|---|---|---|
| a server's address | `servers.json` | [server-file.md](server-file.md) |
| a model's layout (names, hours, files) | `<name>.model` | [model-files.md](model-files.md) |
| a map (fill, lines, barbs) | `<name>.chart.json` | [chart-files.md](chart-files.md) |
| an ensemble or a variable of the place windows (percentiles, two models compared) | `ensembles.json` | [ensemble-file.md](ensemble-file.md) |

## Which file for which job

| the job | edit |
|---|---|
| A server moved (a bucket renamed, `para` became `prod`) | **a server's base** in `servers.json`, or type it in Settings > Model Sources > Servers |
| A server has a mirror to fall back to | a second base for that server in `servers.json` |
| A file is now named differently, or its layout changed | the model's `.model` file (`url`, `file` rules) |
| A variable was renamed (`TMP` is now `T`) | the model file's `nc_variable` rule (NetCDF) or file rule; the check below tells you which |
| The model got a new level or variable you want | a **chart file** that reads it (or the model's `clone_fields`) |
| You want a map the program does not have | a **chart file** |
| A chart cannot draw on a model | the catalogue check says which field the model lacks: remove the level or field from that model's `clone_levels` / `clone_fields`, or leave it |

A chart that needs a calculation (potential vorticity, an index) cannot be made from a file: those are part of the program. A chart file draws the fields a model carries, or the speed, difference or mean of two of them.

## Where the files go

One folder, the same for all three:

    <app data>/models/        (Linux: ~/.local/share/<org>/<app>/models)

Settings > Model Sources > **Your file...** opens it. The project's own copy of the server list is kept beside it
(`<app data>/servers/project.json`, written by the program: do not edit it, edit `servers.json` in the models folder instead).

## The rules every file follows

1. **Plain text, UTF-8.** Model files are `key = value` lines with `#` comments; the server and chart files are JSON
   (`"version": 1` first). No comments in JSON: put a `"note"` where a place has one.
2. **All or nothing.** A file with one error is not used at all (a model file leaves the model out, a chart file all its charts,
   a server file the whole layer), so a typo never half-applies. What stands then is the layer below: the built-in table, the
   built-in model, no chart.
3. **The reason is shown.** The list of model problems (Settings > Model Sources) names the file, the line and what is wrong.
   Look there first when something does not appear.
4. **Narrow names.** Ids are letters, digits, `_`, `-`. A web address is `https://` (or `http://` in a model file) with the host
   written out: no `user@`, no spaces, no quotes, no `..` as a folder, no GDAL `/vsi` paths. Fields are `VARIABLE@level`. A
   model file may begin a `url` with `{@server-name}` to use a server from the table; the table's own addresses are checked
   when it is read.
5. **Later layers win, by name.** For servers: built in, then the project's file, then **yours**, then what you type in Settings.
   For models and charts: yours go **after** the built-in ones, and an id the program already has is **not** replaced (a model
   file that repeats a built-in id is left out; a chart whose id a model already has is left out). Use a new id.
6. **You can break your own, not the program's.** Nothing you write changes what the program itself does or what a built-in
   chart looks like. Delete the file and the program is as it was.
7. **Read when the program starts**, except: a base typed in the Servers table and the project's file (fetched once a day, or
   "Fetch now") take effect at once; a changed `servers.json` of yours is read when either of those happens or at the next start.
   Model files and chart files: restart.
8. **Nothing about you leaves the computer.** The only request the program makes about all this is the project's server list,
   once a day, with nothing in it but a generic name; the switch is in Settings > Model Sources, and off means only the built-in
   list and yours.

## Finding out what to fix

- **Is a server up, and which layer is it from?** Settings > Model Sources: the **Servers** table (name, the places in order,
  the layer each came from) and the check of each model's newest run below it.
- **Did a model's files change?** The catalogue check compares every field the program's charts ask for with what a model's
  newest file lists:

      WXQT_OPEN=catalogcheck:<model, or *>:<out.json>      (QT_QPA_PLATFORM=offscreen for no window)

  It prints a line for each model and writes the lists. **Gone** (asked for, was there last time, not now) is a rename or a
  removal: fix the model file. **Never found** is a chart that has never worked on that model. **Not cataloged** is a field the
  file lists that no chart reads: the fields you could chart. A model with no index (Open-Meteo, Zarr, tar) is reported as not
  checked, with why.
- **Why does my chart not draw?** The model does not list a field it reads; the message on the chart names it.

## Examples

A server moved (`servers.json` in the models folder):

```json
{ "version": 1,
  "servers": { "rrfs-ops": { "bases": ["https://new-bucket.s3.amazonaws.com"] } } }
```

A new map (`mymap.chart.json`): 850 mb temperature as the area, 850 mb heights as lines:

```json
{ "version": 1,
  "charts": [ { "id": "t850_z850", "model": "GFS", "label": "850 mb temperature and height",
                "fill": { "field": "TMP@850 mb", "ramp": "temperature", "quantity": "temperature", "title": "850 mb temperature" },
                "contours": [ { "field": "HGT@850 mb", "scale": 0.1, "interval": 3, "title": "Height (dam)" } ] } ] }
```

A model of your own is a `.model` file: copy a built-in one from `src/gfs/GfsBuiltInModels.cpp` and change it.

## Not yet (planned, see `plans.md` sections 20 and 21)

An **override file** that changes only some keys of a built-in model (today a built-in model can be replaced only by a model with
another id); the project **publishing** server and chart additions so a fix reaches everyone without a release (the server
list is published already; its signature is not); a Settings page listing your files and their problems; "make a chart from
this field" from the catalogue check; reloading without a restart.
