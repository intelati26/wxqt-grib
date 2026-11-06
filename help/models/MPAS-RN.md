# MPAS-RN

*NSSL experimental MPAS-RN, 3 km*

## Why it matters

A second research run of the MPAS model from NOAA's National Severe Storms Laboratory, at 3 km, posted as the "mpasrn" files on the NSSL data server (00Z and 12Z, about 15 hours after the run time). Per NSSL's CAMs page it is started from the experimental 00Z and 12Z deterministic RRFS run of NOAA's EMC and uses the NSSL 2-moment microphysics (the "mpasrn3" files are the same run with the 3-moment scheme). Experimental research output: not for forecasting.

## Further reading

- [NSSL CAMs: MPAS runs and how they are set up](https://cams.nssl.noaa.gov/) (From the people who run it)
- [Copernicus CAMS: the air quality service, a different CAMS](https://atmosphere.copernicus.eu/) (From the people who run it)

## Where the data is read from

- [NSSL data server (FRDD/NSSL-MPAS)](https://data.nssl.noaa.gov/thredds/catalog/FRDD/NSSL-MPAS/2026/catalog.html)
- [NSSL data server, all FRDD model folders (MPAS tars are <yymmddcc>_mpasht2.tar and _mpasrn.tar; browse with an honest User-Agent, the server refuses curl's)](https://data.nssl.noaa.gov/thredds/customConfig/FRDD.xml)
