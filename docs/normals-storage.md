# Climate normals: what to download and what to keep

The source is NOAA's U.S. daily gridded climate normals for 1991-2020 (NCEI Accession 0259962, 1/24 degree, about 5 km, contiguous U.S.),
served by Planetary Computer as one cloud-optimized GeoTIFF per day of a 366 day year and per variable
(`.../gridded-normals-cogs/normals-daily/1991-2020/1991_2020-daily-NNN-<tmax|tmin|tavg|prcp>_norm.tif`, a short-lived anonymous token from
`https://planetarycomputer.microsoft.com/api/sas/v1/token/noaanormals/gridded-normals-cogs`). Each file is about 0.87 MB (float32, DEFLATE, with overviews), the year 319 MB, per variable.
Day 59 is Feb 28, day 60 Feb 29 (their calendar always has the leap day), day 314 is Nov 9.

## The rule

**Never download or keep the whole year.** The normal is a smooth seasonal curve, so:

1. Fetch 15 sample days (every 26th: 1, 27, 53, ... 365), about 13 MB, one request a second.
2. Fit a mean plus 6 sine/cosine pairs per land cell (13 numbers, over the 469,758 land cells).
3. Keep only the coefficients, as 16 bit steps of 0.05 F, differenced along the cell order and zlib-compressed: **about 2.6 MB per variable**. Delete the daily files.
4. The normal for any date is `c0 + sum(c[2h-1] cos(h t) + c[2h] sin(h t))`, `t = 2 pi (day - 1) / 366`: no further download.

`resourceCreation/createNormals.py` does steps 1-3 and `--verify` checks against any daily files that are around.

## What it was measured to cost (tmax, every one of the 366 days, 469,758 cells)

| samples | files | MB to fetch | rms (F) | 99.9th pct | worst |
|---|---|---|---|---|---|
| every 15th day, 6 harmonics | 25 | 22 | 0.073 | 0.35 | 0.56 |
| **every 26th day, 6 harmonics** | **15** | **13** | **0.080** | **0.34** | **0.65** |
| every 30th day, 6 harmonics | 13 | 11 | 0.113 | 0.48 | 0.76 |
| every 15th day, 5 harmonics | 25 | 22 | 0.304 | 0.93 | 1.28 |
| all 366 days, 6 harmonics (the floor) | 366 | 318 | 0.060 | 0.25 | 0.40 |

Seven or eight harmonics gain nothing (the data are not exactly harmonic); five lose too much. The worst day is always about Feb 28 - Mar 1 (the leap day seam).
The written file (`tmax_norm.nrm`, 2.62 MB) read back against all 366 daily files: rms 0.089 F, worst 0.68 F. Against 14 NCEI station normal highs for Nov 9: mean +0.1 F, rms 1.0 F
(the app's old 2.5 degree mean + 9 was -3.2 F with an sd of 4.2 F; the difference left is the station against its 5 km cell, Denver +2.3 the most).

## Other variables

`tmin_norm` and `tavg_norm` behave like `tmax_norm`. Precipitation is bumpy (zero-bounded, with a smoothed daily curve): check it with `--verify` before trusting six harmonics, or keep the 12 monthly files (about 10 MB).
The `_std`, `_min`, `_max` files are monthly or annual statistics in the same collection, not daily: fetch them as they are.

## Outside the contiguous U.S.

The grid stops at the border; keep `GfsClimate` (the 2.5 degree NCEP reanalysis) as the fallback there.
