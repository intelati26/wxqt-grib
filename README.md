[TOC]
# wxqt: Qt/C++ port of "wX" US Advanced Weather application (open source GPL3)

This is a fork of [joshua.tee/wxqt](https://gitlab.com/joshua.tee/wxqt) - all
credit for the original app and port goes to Joshua Tee; see below for what's
different here.

[screenshot](https://gitlab.com/joshua.tee/wxqt/-/blob/main/wxqt.png)

## What this fork adds

This fork ([intelati26/wxqt-grib](https://github.com/intelati26/wxqt-grib)) keeps everything in the original app and adds
model, ensemble, tropical and climate tools, native sounding analysis, and a packaged build for Windows and Linux. Everything
below is in the toolbar (group names in the grouped toolbar style are in brackets).

### Models and ensembles  [Models]
- **Model Viewer** (Ctrl-M) - model charts drawn here from NOAA's and ECMWF's open GRIB data (GDAL decodes, wxqt draws): GFS, GEFS (mean, spread,
  member probabilities, spaghetti), the National Blend, RRFS and its 5-member ensemble REFS, AIGFS, ECMWF IFS / AIFS, HAFS, GFS-Wave and GEFS-Wave,
  with the colour scales and layout of NCEP's model guidance pages at a higher resolution. About 600 charts in a **grouped, searchable chart
  picker** (favourites, and lines or wind barbs ticked onto a chart), a grouped model and area picker, **hover read-out** and **zoom / pan**
  (wheel, Ctrl + / Ctrl -), a **timeline** with the hours that are ready, read-ahead of the hours to come (12 / 24 hours or the whole run), a
  loop, **change since an earlier run**, **maximum over 24 hours or Day 1**, saved views, a click for a **sounding**, the newest and
  yesterday's runs marked, and Save (the picture, the loop or a whole comparison). Only the box of a region is downloaded where NOAA's server can
  cut one. Fields are kept for 48 hours (2 GB) in the cache; Settings shows its size.
- **Compare tiles** - 1 x 2, 1 x 3 or 2 x 2 charts of one hour: other charts of the model, the same chart from other models, or the same valid time
  from older runs, with one zoom, pan and read-out across them and Play stepping through the hours drawn in every tile.
- **Build a chart** - maps made to order from the members of the GEFS: the chance of rain over a limit in a period of 6 to 240 hours (or under
  it), of a precipitation type, a temperature, dew point, heat index, humidity, wind, gust, pressure, precipitable water, cloud, CAPE or height over
  or under a limit, of an anomaly of so many standard deviations, and "heavy rain potential" (precipitable water, MUCAPE and rain together). The
  GEFS also has 10th / 50th / 90th percentile maps and an extreme forecast index. The REFS group has the mean, spread and probability charts of the
  old SREF screens, and SHIP and STP are RRFS charts.
- **Network window** (Ctrl-Shift-N, or click the activity line) - every download the program has going or waiting, who is waiting for it, and
  what was retried. One persistent network client (a few connections to each server, retries, "on screen" before "read ahead").
- **SPC REFS** - SPC's experimental REFS ensemble read from its open Zarr data (no GDAL): product by SPC's own titles, cycle, time,
  member, probabilities, paintball plots, updraft helicity, loop, hover and crosshair read-out, and the standard SPC mesoscale
  sectors as cropped, smoothed zooms. Saves with SPC-style information bars.
- **REFS ensemble viewer** - 4-panel mean / spread / probability / paintball comparison of the RRFS ensemble, linked hover,
  point plume chart, export of the whole comparison.
- **Parametric index viewer** - SHIP and STP computed locally (no Python), with max-over-range and run-to-run change maps (the same two
  indices are now charts in the Model Viewer).
- **SPC Post slideshow**, **NSSL CAMs** (MPAS, WRF, HRRR, RRFS and others, with loops), **NSSL WRF**, **SPC HRRR**,
  **WPC GEFS**, and the **ESRL** and **WPC GEFS** picture viewers (the "Image models" menu of the Model Viewer).

### Soundings  [Forecast and observations / Models]
- A native sounding engine and viewer (SHARPpy algorithms ported to C++, BSD licence, see `licenses/sharppy-notice.md`) for SPC's
  observed soundings and for **model soundings** built from an RRFS column at a point (or an area mean).
- Skew-T on SPC's axes with the usual annotations, SPC-style hodograph, wind-speed, storm-relative wind, theta-e, inferred
  temperature advection and STP / SHIP box-and-whisker panels, parcel / kinematic / composite index tables (STP, SCP, SHIP,
  DCAPE, K index, Corfidi vectors, critical angle, BRN shear, ...), the tornado probability box, and **SARS analogues**
  from SPC's databases. Checked against SPC's own graphics for OUN and FWD.
- SPC's 1180 x 968 layout by default (or a layout that fills the window); Save exports on a white background at twice size.

### Radar  [Radar and satellite]
- **MRMS viewer** - the full MRMS product list (reflectivity, hail MESH / POSH, rotation, azimuthal shear, rain rate and QPE,
  echo tops) at native resolution on the map, with scan picker, loop, zoom about the pointer, value read-out, Save. The map's
  overlays (warnings, watches and discussions, outlooks, fronts, observations) are the ones switched on under Settings > Map.
- A MRMS still can be a picture on the home screen (Settings > Home Screen Order).
- **MRMS is the only radar.** The single-site NEXRAD (Level III) radar windows were removed from this fork. For raw radar files
  (Level II / III, single-site base products and velocity) use [Supercell Wx](https://github.com/dpaulat/supercell-wx).
- **Rivers** - the NWS river gauges (about 13,000) on the same map, coloured by flood category; click a gauge for its hydrograph
  with the NWS forecast and the National Water Model, now / modelled / record statistics and the flood impacts.

### Tropical  [National, tropical and marine]
- **Tropical screen** - every active tropical cyclone in the world (CIRA / RAMMB's list) by basin with a current infrared picture;
  the NHC tool (Atlantic, East and Central Pacific outlooks, advisories, SST) is a child of it.
- **Storm pages** - satellite and guidance products, infrared and 89 GHz microwave loops, forecast and track history,
  rapid-intensification tables; a CIRA button in NHC's own storm window.
- **Track map and recon** - the NHC track, the cone, watches and warnings and the model guidance, with the reconnaissance: flight tracks coloured by
  the wind with barbs, the centre fixes of the vortex messages and the dropsondes (click one for its profile), for **the last 3 to 48 hours** or all
  of it. The one-flight page (the aircraft on the storm's own satellite picture) draws the same way and has the same options (hours, centre fixes,
  dropsondes, labels, barbs); the labels keep off each other.
- **Western Pacific, Indian Ocean, Southern Hemisphere**: JTWC's warning text, forecast reasoning and warning graphic (or the
  basin outlook for an invest) and **JMA's analysis and forecast in English** (composed from JMA's data feed).

### Climate and ocean  [National, tropical and marine]
- A dashboard: the **ENSO Alert System status** and synopsis from CPC's diagnostic discussion (El Nino / La Nina watch,
  advisory, ...), bar charts of **RONI, ONI, the Nino regions, SOI, PDO, NAO, AO, PNA and AAO**, and picture sections:
  **sea surface temperature (raw)** and **SST anomaly** (global, regional, NHC's Atlantic and Pacific), marine heat (HotSpots,
  Degree Heating Weeks, bleaching alert), **ocean currents** (Global RTOFS and NCOM, OPC), CPC's ENSO figures with the subsurface
  and the official outlook, and tropical convection (MJO).
- **History and loops** for SST, anomaly, HotSpots, DHW and bleaching alert: a date picker back to 2020-01-01, frame count and
  spacing (daily / weekly / 30 days), play / scrub / save. NHC's 14-day SST loops too.
- Source tables (RONI, ONI, Nino indices, SOI, PDO, NAO, AO, MJO, ocean heat content) open as text.

### Drought  [National, tropical and marine]
- The **U.S. Drought Monitor** from its own map shapes, drawn sharp at any zoom over the states and counties, for the country, a state, a county, an NWS
  forecast office or an SPC map area; the change between two weeks; the share of the area in each category by week and a table, with the
  **history back to 2000** (rain, temperature, rank against every year on record, drought shares) kept as plain CSV files you can read and add to.
  Precipitation totals, departures and percent of normal, the outlooks and soil moisture, and a chart and table of the chosen area's last
  twelve months. The charts and the map save as pictures.

### Forecast point  [Home screen]
- The home screen's **forecast point** card (as the NWS "IDSS Forecast Points" page): the week at a glance of your location (highs and lows, wind, gusts,
  chances of rain and thunder, dew point, humidity, cloud) and what the SPC and WPC outlooks say of the point for the next three days. Its full
  page has every row, the hourly graph of any series (or all of them), the hourly table with CSV export, the office's forecast discussion and
  any saved location or point of your own. The master map has a layer of your saved locations. Settings > "Show the forecast point".

### Severe weather and forecasts
- Severe outlook comparison (SPC / CSU-MLP / Missouri ABPG, days 1-8), SPC outlook summaries, mesoanalysis, storm reports,
  RTMA, observations, lightning, national images, OPC and the other original screens.

### Saving, exporting and the renderer
- One Save for every picture and loop: animated PNG always; **JPEG XL, AVIF, WebP and MP4** when their free tools are installed
  (a "how to add them" help is in the Save dialog; the Windows package bundles `img2webp`). Files default to
  `yyyyMMdd_HHmmZ_product` using the time the server says the picture was produced; model images carry run / hour / valid time and
  SPC-style header bars. Right-click "Save image..." on any picture.
- The **Animation renderer** window: Save asks for the file and format, then the encode runs in the background, one job at a time,
  with waiting / rendering / done / failed status, so a long encode never locks the program up.

### Home screen, toolbar and settings
- A visual **home-screen layout editor** (zone templates; drag Severe / Thumbnails / Forecast / Text), reorderable and groupable
  toolbar with three styles (icons, icons + names grouped, menu bar), fixed-size wrapping thumbnails (GRIB latest-run render,
  SPC sounding, MRMS radar, NHC outlooks), middle-button drag to pan.
- System / Light / Dark theme; each window remembers its size and position.
- **Contact email**: NWS asks API users to identify themselves, so the program asks for an email at each start until one is saved
  (Settings > General). It is sent only in the HTTP User-Agent, nowhere else.

### Builds and diagnostics
- **GitHub Actions** builds a **Windows x64 portable zip** (MSVC, minimal bundled GDAL, `img2webp`) and a **Linux x64 AppImage**
  on every push (`.github/workflows/build.yml`); the artifacts are on the run's page. A push builds in about four minutes: the compiler runs through
  sccache (the compile cache is kept in GitHub's cache; the Windows build is one `cl` per file and uses `jom`), so only the files that changed are
  compiled again. The default build is "lite" (QtWebEngine is
  optional: `./makeAll.py --webengine`).
- `wxqt.log` records start / exit, uncaught exceptions and Windows crash details; `--debug` / `WXQT_DEBUG=1` adds a debug trace.
  Settings on Windows live in an INI file next to the program when it is writable.
- Development aids (run with `QT_QPA_PLATFORM=offscreen`): `WXQT_OPEN=<toolbar id>` opens a tool, `WXQT_GRAB=<file.png>[,<ms>]`
  saves a picture of it and quits, `WXQT_SIZE=<w>x<h>` sizes it.

### Data sources and credits
NOAA / NWS (NHC, SPC, WPC, OPC, CPC, NESDIS OSPO and Coral Reef Watch, NCEP, NSSL, MRMS), CIRA / RAMMB (Colorado State),
JTWC, JMA, ECMWF open data (CC BY 4.0), NOAA PSL (the reanalysis for the climatology), the U.S. Drought Monitor, AWS and Google Cloud open data (mirrors). SPC's REFS, CIRA's products and several NSSL models are **experimental**; this is not a
warning service - see the disclaimer below. Sounding algorithms: SHARPpy (BSD), see `licenses/sharppy-notice.md`.
zstd (BSD / GPLv2) is vendored for the Zarr reader, see `licenses/zstd-LICENSE.txt`. GDAL (MIT), PROJ (MIT), ECMWF ecCodes (Apache 2.0), Qt and the
data providers are credited in `licenses/CREDITS.md`.

Prerequisites:
* Qt 5.12 or higher (Qt 6 is what the packages are built with)
* C++ compiler supporting C++17 (most modern Linux distributions are fine)
* Tested on the Linux distros mentioned below, the current version of macOS, and Windows 10. (as of Dec 2021)
```
wxqt is an efficient and configurable method to access weather content from the NWS, NSSL WRF, and blitzortung.org.
Software is provided \"as is\". Use at your own risk. Use for educational purposes and non-commercial purposes only.
Do not use for operational purposes.  Copyright 2020, 2021 joshua.tee@gmail.com .
Privacy Policy: this app does not collect any data from the user or the user’s device.
Please report bugs or suggestions via email."
wxqt is licensed under the GNU GPLv3 license. For more information on the license please go here:"
https://www.gnu.org/licenses/gpl-3.0.en.html
```

## For those interested in forking or running a modified program:
Please modify `GlobalVariables::appName` in `src/common/GlobalVariables.cpp`
(used in HTTP requests to the NWS). The contact email sent alongside it is
no longer hardcoded - it's blank by default and set per-install from
Settings > General > "Contact email for weather API requests," so nothing
personal needs to be edited in source at all.
FYI - you will notice that I've abstracted the native toolkit widgets. This was done as non-public ports to other UI tookits share this codebase, etc.

## Differences from mobile versions (similar in content to wXL23 but native desktop with keyboard shortcuts, etc):
- Single-site NEXRAD radar (Level II or III) is not included in this fork; MRMS is the radar here, and Supercell Wx handles raw radar files.
- No notifications or widgets
- Prebuilt Windows and Linux packages come from this repository's GitHub Actions (or compile it yourself)
- Best effort support from me (ie Mobile support takes priority)

## How to add your location
- From the main screen, tap the gear icon in the upper left.
- From the Settings window, tap the "Add Location" tab.
- Enter the name of your city in the text box, as type you will start getting matches. The best match will auto populate the name/lat/lon fields.
- If you want a differen result, tap button that most closely matches your location.
- Tap the save button, when all fields clear the new location has been saved.
- Close settings window
- From the main screen use the drop down to choose your new location

## FYI - output to local filesystem (file should NOT exist before running program for first time):
- MacOS (standard pref spot): `$HOME/Library/Preferences/com.tee@gmail.joshua.wxqt.plist`
- Linux distro: `$HOME/.config/joshua.tee@gmail.com/wxqt.conf`

## Bugs (that might never get fixed)
* At times if a thread gets stuck it will not exit properly, **recommendation is to always start program from command line**.

## Help
From the main screen and the map screens do `Ctrl-/` (? key) to get keyboard shortcuts. Mouse over on some icons will sometimes show a label or shortcut as well.

## Compile and run
1. Perform the [steps](https://gitlab.com/joshua.tee/wxqt/-/blob/main/README_OS.md) for your operating system, you will probably need 8GB of memory for compilation. I have used a 4GB Raspberry PI 400 (keyboard model) to compile.
2. Download the code and and compile/run
```bash
git clone https://gitlab.com/joshua.tee/wxqt.git
cd wxqt
# NOTE: Windows requires additional steps, please see README_WINDOWS.md
./makeAll.py --qt5
# or for qt6
./makeAll.py
```
3. After compilation you can simply launch with script
```bash
./run.bash
```
## Qt 6 note (TODO):
- need to add instructions but use "qmake6" instead
- need 
```bash
qt6-base
qt6-declarative
