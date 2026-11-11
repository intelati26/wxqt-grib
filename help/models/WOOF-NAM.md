# WOOF-NAM

*WOOF-NAM, the NAM's NMMB on a GPU (runs on this computer)*

## Why it matters

NCEP retired the NAM on 3 November 2026. WOOF-NAM is a community GPU port of its NMMB dynamics and operational physics that keeps it running. This entry shows its runs on this computer (the nests it makes for an area), so they can be compared with other models and with MRMS at the same valid time.

## The hosted source (WOOF-NAM-HOSTED)

A second entry, `WOOF-NAM-HOSTED`, reads the files Recast publishes at woof-nam.recastsystems.com: NCEP grid 218, 12 km, four runs a day (00, 06, 12, 18Z) to hour 84, the first file about 3.5 hours after the run. Only the fields a chart needs are read (by the index file's byte ranges; a run is 5.9 GB). The data is WOOF-NAM by Recast, CC BY 4.0, and is not an official forecast.

## Further reading

- [WOOF-NAM](https://github.com/recastsystems/woof-nam) (From the people who run it)

## Where the data is read from

- Folders on this computer: `runs/<name>/run` under `~/woof-nam` or `~/Claude/woof-nam`; the setting `WRF_FOLDERS_WOOFNAM` (folders separated by `;`) names others.
