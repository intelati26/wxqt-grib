# Servers by name: `servers.json`

(The rules every editable file shares, and which file to edit for which job: [editing-rules.md](editing-rules.md).)

A model's `url` can begin with `{@name}` instead of a host: `url = {@rrfs-ops}/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{name}.f{hour3}.conus.grib2`.
The name is looked up in a small table of servers, so a server that moves is a change of **one line of one file**, and you do
not have to wait for a new version of the program. (Design: section 20 of `plans.md`.)

## The file

```json
{
  "version": 1,
  "updated": "2026-11-15",
  "servers": {
    "rrfs-ops": {
      "bases": ["https://noaa-rrfs-ops-pds.s3.amazonaws.com",
                "https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com"],
      "note": "what it is"
    }
  }
}
```

- `servers`: names (a to z, 0 to 9 and `-`, 40 characters at most) to `bases`: one to four `https` addresses, no query, no
  fragment, no trailing slash. **The first base is tried first; the others are the fallbacks**, in order (a model's urls
  are tried in order, so a dead first server falls through to the next).
- The whole file is at most 64 KB. A file that is not valid in any entry is **not used at all** (the layer below stands) and
  the reason is shown in Settings > Model Sources.

## The layers (the later over the earlier, by name)

1. **Built in**: `servers/servers.json` of the repository, compiled in (`python3 servers/make_builtin.py` writes
   `src/gfs/ServersBuiltIn.cpp` from it; a test checks they are the same). The program works with nothing else.
2. **The project's file**: the same file in the repository (`main`), read once a day and kept
   (`<app data>/servers/project.json`). Settings > Model Sources > "Read server updates from the project" switches it off.
   Only hosts the program reads from today are accepted in it (the suffixes of `Servers::allowedHost`: AWS, NOAA, Google,
   NASA, DWD, ECCC, Open-Meteo, ECMWF, UCAR, Meteo-France), unless "Allow other hosts in the project's file" is on.
3. **Yours**: a `servers.json` in the folder of your own models (`<app data>/models/`: Settings > "Your file..." opens it),
   same format, any host. It goes over the project's.
4. **Settings**: a base typed in the Servers table (kept as `SERVER_<name>`), put first.

Each row of the Servers table in Settings says which layer it came from.

## Fixing a moved server

- **You, now:** put the new base in your own `servers.json`, or type it in the table. No restart for the table; a chart
  picks it up when it next asks for a file.
- **Everyone:** edit `servers/servers.json` in the repository (and run `python3 servers/make_builtin.py` so the built-in
  copy matches), commit, push. Every install that reads the project's file has it within a day.

What this does **not** cover: a change of what is in the files (a renamed variable, a new layout). That is an edit of a model
file (`docs/model-files.md`); see the plan's section on alerts for such changes.

## Used by

Built-in models that use `{@name}` today: `rrfs-ops` (RRFS, REFS, the RRFS ensemble, the fire-weather RRFS), `gfs-aws` (GFS),
`ecmwf-gcs` (IFS, IFS-ENS, AIFS), `geos-nccs` (GEOS-FP). Other models still carry their hosts in their own `url` lines, and a few
addresses are still in code (`URL.cpp`, `HurricaneData.cpp`, `UtilityGrib.cpp`): converting them is the same one-line change.

## The servers the program knows

`rrfs-ops`, `gfs-aws`, `gefs-aws`, `nbm-aws`, `cfs-aws`, `hrrr-aws`, `aigfs-aws`, `mrms-aws`, `rtma-aws`, `urma-aws`, `era5-ncar`, `nomads` (NOAA's operational server: HAFS, HREF,
SPC post-processing and the older RRFS paths), `ncei-models` (the archives), `ecmwf-gcs`, `dwd-opendata`, `msc-datamart` (Environment Canada), `openmeteo-s3` (the `.om` files),
`openmeteo-seasonal`, `nssl-thredds`, `spc-refs`, `wpc-ppp`, `woof-nam` and `geos-nccs`. Every model's `url` begins with one of them, and so do the few addresses the program builds in code
(the observation and bulletin readers, the radar mosaic listings, the older RRFS viewers): moving a server is one entry. The Settings > Model Sources > Servers table lists them with their state.
