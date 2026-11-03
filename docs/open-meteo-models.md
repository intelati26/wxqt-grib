# Open-Meteo open data: the models and where they would sit in the Model Viewer

Surveyed 2026-11-03 from `https://openmeteo.s3.amazonaws.com/data_spatial/<folder>/latest.json` (one small file per model: the run, whether it is complete, the valid times, the variable names and the grid as WKT). The files are `.om` files read by plain HTTPS range requests, which do not count against the API allowance (see `docs/plans.md` item 5 for the reader, a separate helper program, and the licence).
The runs read were the 2026-11-03 ones, complete, except EC46: its `latest.json` still named the 2026-10-29 run while the 3 November run was being written in its folder (`completed` false), so the file for the newest completed run is what `latest.json` gives and the reader must not look past it.

What a folder holds is **the variables Open-Meteo keeps**, not the provider's whole output: surface fields, clouds, precipitation, CAPE, gusts, and pressure-level fields for the larger models. Two things to know before relying on one: **radar reflectivity** is there only for RRFS (`radar_reflectivity`), not HRRR or the others, and the ensembles are **not** in members: a folder of an ensemble has the probability (`precipitation_probability`) or the mean and spread (the hurricane GEFS), so the viewer's member charts and paintballs stay on the GRIB sources.

**Grids** (how much work each is for the reader): regular lat/lon (most; a copy), rotated lat/lon (the Canadian, the Swiss and KNMI's European models; GDAL's warp), Lambert conformal (HRRR, NAM, NBM, RRFS, the Danish and the Norwegian models; the warp), Lambert azimuthal (the UK 2 km), reduced Gaussian (the ECMWF's O320, O1280 and the European N320 area ones; needs the Gaussian latitudes and an interpolation), **ICON native** (DWD's icosahedral grid: skipped, the regular lat/lon `dwd_icon` and `dwd_icon_eu` are enough).

The viewer's groups today: Global, Seasonal, Regional, Convection allowing, Ensembles, Waves and ocean, Tropical cyclones, SPC guidance and mesoanalysis, Archive. Proposed new groups: **Air quality** (CAMS) and **Other regions** (Europe, Asia, kept at the bottom so they do not crowd the picker of a US user).

## Seasonal (the one that started this)

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `ecmwf_ec46_weekly` | EC46 (the model that is there now; the map from the files instead of the API) | 96-936 h, weekly | 52 | reduced Gaussian O320 | 50 variables: the mean and the anomaly of temperature, precipitation, pressure, wind, SST, soil, snow, cloud, and the probabilities of anomalies, the EFI and shift of tails. SEAS5 has no grid files and stays on the API |

## Global (new to the viewer)

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `ukmo_global_deterministic_10km` | UKMO (new) | 0-168 h, 1 h | 216 | regular lat/lon | UK Met Office global; 216 variables, pressure levels 10-925 hPa and more |
| `meteofrance_arpege_world025` | ARPEGE (new) | 0-102 h, 1 h | 174 | regular lat/lon | Meteo-France global, 0.25 degrees; 174 variables |
| `dwd_icon` | ICON (new, the global one; ICON-EU and ICON-D2 are in) | 0-120 h, 1 h | 123 | regular lat/lon | DWD global on a regular grid; 123 variables |
| `jma_gsm` | JMA-GSM (new) | 0-132 h, 6 h | 78 | regular lat/lon | Japan's global; 78 variables, 6 hourly |
| `cma_grapes_global` | GRAPES (new) | 0-120 h, 3 h | 258 | regular lat/lon | China's global; 258 variables, 3 hourly |
| `cmc_gem_gdps_15km` | GDPS (new) | 0-240 h, 1 h | 23 | regular lat/lon | Environment Canada's global, 15 km; the upper-air fields are the folder `cmc_gem_gdps_15km_upper_level` (155 variables) |
| `ecmwf_ifs025` | IFS (in; a second source) | 0-360 h, 3 h | 121 | regular lat/lon | the 0.25 degree model, also on `ecmwf_ifs` (O1280, hourly to 144 h) |
| `ncep_gfs025` | GFS (in; a second source) | 0-384 h, 1 h | 316 | regular lat/lon | 316 variables; also `ncep_gfs013` (the 13 km surface set) |
| `ecmwf_aifs025_single` | AIFS (in; a second source) | 0-360 h, 6 h | 103 | regular lat/lon |  |
| `ncep_aigfs025` | AIGFS (in; a second source) | 6-384 h, 6 h | 87 | regular lat/lon |  |

## Convection allowing and Regional

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `ncep_rrfs_conus` | RRFS (in; a second source) | 0-84 h, 1 h | 307 | lambert conic conformal | the one folder with **radar_reflectivity** among the NCEP models; 307 variables; `ncep_rrfs_north_america` is the North America domain on a rotated grid |
| `ncep_hrrr_conus` | HRRR (in; a second source) | 0-18 h, 1 h | 263 | lambert conic conformal | no reflectivity here; the run read (20Z) reaches 18 h; the 15 minute folder `ncep_hrrr_conus_15min` has 12 surface variables |
| `ncep_nam_conus` | NAM (retired by NOAA on 3 Nov 2026 and in Archive; check whether this folder goes on) | 0-60 h, 1 h | 33 | lambert conic conformal |  |
| `ncep_nbm_conus` | NBM (in; a second source) | 1-262 h, 1 h | 19 | lambert conic conformal | 19 variables, with the probabilities of precipitation and snowfall |
| `cmc_gem_rdps_10km` | RDPS (new, Regional) | 0-84 h, 1 h | 179 | rotated lat/lon | the Canadian regional, 10 km, rotated grid; 179 variables |
| `cmc_gem_hrdps` | HRDPS (in; a second source) | 0-48 h, 1 h | 164 | rotated lat/lon | the Canadian 2.5 km; `cmc_gem_hrdps_west` is the west domain |
| `dwd_icon_eu` | ICON-EU (in; a second source) | 0-30 h, 1 h | 121 | regular lat/lon |  |
| `dwd_icon_d2` | ICON-D2 (in; a second source) | 0-48 h, 1 h | 93 | regular lat/lon |  |

## Ensembles (probabilities and mean, not members)

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `ncep_gefs025` | GEFS (in) | 0-240 h, 3 h | 1 | regular lat/lon | only the precipitation probability; the viewer's GEFS stays on its GRIB |
| `ncep_gefs05` | GEFS 0.5 degree (in) | 0-384 h, 3 h | 1 | regular lat/lon | same, to 384 h |
| `cmc_gem_geps` | GEPS (in) | 0-384 h, 3 h | 1 | regular lat/lon | only the precipitation probability |
| `ncep_aigefs025` | AIGEFS (new) | 6-384 h, 6 h | 1 | regular lat/lon | one field; the AIGEFS ensemble mean or probability only |
| `ecmwf_ifs025_ensemble` | IFS-ENS (in) | 0-144 h, 3 h | 1 | regular lat/lon | one field |
| `ukmo_global_ensemble_20km` | UKMO ensemble (new) | 0-246 h, 1 h | 1 | regular lat/lon | one field (a probability) |

## Tropical cyclones

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `ncep_hgefs025_ensemble_mean` | HGEFS mean and spread (new) | 6-240 h, 6 h | 144 | regular lat/lon | the hurricane-model GEFS: the mean and the spread of temperature, precipitation, pressure, cloud and the pressure levels to 1000 hPa; 6 hourly to 240 h |

## Waves and ocean

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `ncep_gfswave025` | GFS-WAVE (in; a second source) | 0-384 h, 1 h | 15 | regular lat/lon | 15 variables; `ncep_gfswave016` is the finer grid |
| `ecmwf_wam025` | ECMWF WAM (new) | 0-360 h, 3 h | 4 | regular lat/lon | 4 variables, 0.25 degrees; `ecmwf_wam` is the O1280 grid |
| `dwd_gwam` | GWAM (new) | 0-174 h, 3 h | 11 | regular lat/lon | DWD's global wave model; `dwd_ewam` is the European |
| `meteofrance_wave` | MFWAM (new) | -21-240 h, 3 h | 12 | regular lat/lon | 12 variables, 3 hourly, from 24 h back |
| `meteofrance_sea_surface_temperature` | Meteo-France sea surface temperature (new) | -18-240 h, 6 h | 1 | regular lat/lon | one field |
| `meteofrance_currents` | Ocean currents (new) | -24-239 h, 1 h | 4 | regular lat/lon | 4 fields (the currents) |

## Air quality (a new group)

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `cams_global` | CAMS global (new) | 0-120 h, 1 h | 15 | regular lat/lon | the Copernicus air quality forecast: 15 variables, hourly to 120 h |
| `cams_global_greenhouse_gases` | CAMS greenhouse gases (new) | 0-120 h, 3 h | 3 | regular lat/lon | 3 fields, 3 hourly |
| `cams_europe` | CAMS Europe (new) | 0-96 h, 1 h | 25 | regular lat/lon | 25 variables, hourly to 96 h |

## Other regions (a new group, at the bottom)

Europe and the Far East; none covers the United States. Listed so the choice is made once, not to be built first.

| Open-Meteo folder | In the viewer as | Leads and step | Variables | Grid | Note |
|---|---|---|---|---|---|
| `jma_msm` | JMA-MSM | 0-39 h, 1 h | 11 | regular lat/lon | Japan's mesoscale; upper levels in `jma_msm_upper_level` |
| `meteofrance_arome_france0025` | AROME France | 0-51 h, 1 h | 149 | regular lat/lon | and the HD and 15 minute folders |
| `meteofrance_arpege_europe` | ARPEGE Europe | 0-102 h, 1 h | 17 | regular lat/lon |  |
| `ukmo_uk_deterministic_2km` | UKMO UK 2 km | 0-12 h, 1 h | 386 | lambert azimuthal equal-area | Lambert azimuthal equal-area; 386 variables; the ensemble is `ukmo_uk_ensemble_2km` |
| `knmi_harmonie_arome_europe` | HARMONIE-AROME Europe (KNMI) | 4-59 h, 1 h | 55 | rotated lat/lon | and the Netherlands domain |
| `dmi_harmonie_arome_europe` | HARMONIE-AROME (DMI) | 0-59 h, 1 h | 39 | lambert conic conformal |  |
| `metno_nordic_pp` | MET Norway Nordic | 0-56 h, 1 h | 9 | lambert conic conformal |  |
| `meteoswiss_icon_ch1` | ICON-CH1 / CH2 (MeteoSwiss) | 0-33 h, 1 h | 21 | rotated lat/lon | and the two ensembles (one field each) |
| `geosphere_arome_austria` | AROME and CLAEF Austria | 0-60 h, 1 h | 23 | regular lat/lon | CLAEF is `geosphere_claef_austria` |
| `italia_meteo_arpae_icon_2i` | ICON-2I (Italy) | 0-72 h, 1 h | 69 | regular lat/lon |  |
| `chmi_aladin_central_europe_2km` | ALADIN (Czech Republic) | 0-72 h, 1 h | 123 | lambert conic conformal | and the 1 km folder |

## Order of work (for Thursday and after)

1. EC46 from the files (the reason for all of this); this also builds the helper, the run discovery (`latest.json`) and the reduced Gaussian interpolation.
2. The regular lat/lon globals, which need nothing more than the helper: UKMO global, ARPEGE, ICON, JMA-GSM, GRAPES, GDPS, ECMWF WAM and CAMS (the last is the only one that needs a group of its own).
3. The rotated and the Lambert grids through the warp: RDPS, then the NCEP models as second sources.
4. Other regions only if asked for.

Each model is then a definition file (`kind = om`, `om_model`, and each chart's field by the Open-Meteo variable name) and a line in `pickerGroup`, as the GRIB ones are. Before one is added, read its `latest.json` again for the variable names (they are the service's, and the list changes) and look at one chart against the provider's own for units and sign (wind components, accumulated precipitation, the anomaly baselines of EC46).
