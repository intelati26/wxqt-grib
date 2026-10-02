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
- **RRFS GRIB viewer** - NOAA's RRFS read straight from the GRIB files (field / region / run / forecast hour, loop, run-to-run
  Compare, max-over-range swaths and "Day 1 max" presets, saved views, hover value read-out and crosshair). Falls back to the AWS
  mirror when NOMADS fails.
- **SPC REFS** - SPC's experimental REFS ensemble read from its open Zarr data (no GDAL): product by SPC's own titles, cycle, time,
  member, probabilities, paintball plots, updraft helicity, loop, hover and crosshair read-out, and the standard SPC mesoscale
  sectors as cropped, smoothed zooms. Saves with SPC-style information bars.
- **REFS ensemble viewer** - 4-panel mean / spread / probability / paintball comparison of the RRFS ensemble, linked hover,
  point plume chart, export of the whole comparison.
- **Parametric index viewer** - SHIP and STP computed locally (no Python), with max-over-range and run-to-run change maps.
- **SPC Post slideshow**, **NSSL CAMs** (MPAS, WRF, HRRR, RRFS and others, with loops), **NSSL WRF**, **SPC HRRR / SREF**,
  **WPC GEFS**, **NCEP** and **ESRL** model viewers.

### Soundings  [Forecast and observations / Models]
- A native sounding engine and viewer (SHARPpy algorithms ported to C++, BSD licence, see `docs/sharppy-notice.md`) for SPC's
  observed soundings and for **model soundings** built from an RRFS column at a point (or an area mean).
- Skew-T on SPC's axes with the usual annotations, SPC-style hodograph, wind-speed, storm-relative wind, theta-e, inferred
  temperature advection and STP / SHIP box-and-whisker panels, parcel / kinematic / composite index tables (STP, SCP, SHIP,
  DCAPE, K index, Corfidi vectors, critical angle, BRN shear, ...), the tornado probability box, and **SARS analogues**
  from SPC's databases. Checked against SPC's own graphics for OUN and FWD.
- SPC's 1180 x 968 layout by default (or a layout that fills the window); Save exports on a white background at twice size.

### Radar  [Radar and satellite]
- **MRMS viewer** - the full MRMS product list (reflectivity, hail MESH / POSH, rotation, azimuthal shear, rain rate and QPE,
  echo tops) at native resolution on the radar map, with scan picker, loop, zoom about the pointer, value read-out, Save.
- A MRMS still can replace the live Nexrad tile on the home screen (Settings > Home Screen Order).
- Radar window: resizable, closest-radar button, favourites, Ctrl+click switches to the radar nearest the click.

### Tropical  [National, tropical and marine]
- **Tropical screen** - every active tropical cyclone in the world (CIRA / RAMMB's list) by basin with a current infrared picture;
  the NHC tool (Atlantic, East and Central Pacific outlooks, advisories, SST) is a child of it.
- **Storm pages** - satellite and guidance products, infrared and 89 GHz microwave loops, forecast and track history,
  rapid-intensification tables; a CIRA button in NHC's own storm window.
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
  on every push (`.github/workflows/build.yml`); the artifacts are on the run's page. The default build is "lite" (QtWebEngine is
  optional: `./makeAll.py --webengine`).
- `wxqt.log` records start / exit, uncaught exceptions and Windows crash details; `--debug` / `WXQT_DEBUG=1` adds a debug trace.
  Settings on Windows live in an INI file next to the program when it is writable.
- Development aids (run with `QT_QPA_PLATFORM=offscreen`): `WXQT_OPEN=<toolbar id>` opens a tool, `WXQT_GRAB=<file.png>[,<ms>]`
  saves a picture of it and quits, `WXQT_SIZE=<w>x<h>` sizes it.

### Data sources and credits
NOAA / NWS (NHC, SPC, WPC, OPC, CPC, NESDIS OSPO and Coral Reef Watch, NCEP, NSSL, MRMS), CIRA / RAMMB (Colorado State),
JTWC, JMA, AWS open data (RRFS mirror). SPC's REFS, CIRA's products and several NSSL models are **experimental**; this is not a
warning service - see the disclaimer below. Sounding algorithms: SHARPpy (BSD), see `docs/sharppy-notice.md`.
zstd (BSD / GPLv2) is vendored for the Zarr reader, see `docs/zstd-LICENSE.txt`.

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

## Differences from the original Dec 2021 release
  - color legend stays in one spot
  - zoom in/out in nexrad stays over the spot that is centered
  - current device location circle/dot added in nexrad (off by default)
  - nexrad animations are faster
  - can now change the color of the nexrad background

## Differences from mobile versions (similar in content to wXL23 but native desktop with keyboard shortcuts, etc):
- Nexrad Level 2 is not supported. See the wXL23 [FAQ](https://gitlab.com/joshua.tee/wxl23/-/blob/master/doc/FAQ.md#why-is-level-2-radar-not-the-default) for why I can't provide a good experience with this.
- No notifications or widgets
- No Radar color palette editor
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
* In Nexrad when zooming out of in, it does not stay centered.
* On initial nexrad launch it is not centered on radar site. Usage after this is fine.
* At times if a thread gets stuck it will not exit properly, **recommendation is to always start program from command line**.

## Help
From the main screen and nexrad radar do `Ctrl-/` (? key) to get keyboard shortcuts. Mouse over on some icons will sometimes show a label or shortcut as well.

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
