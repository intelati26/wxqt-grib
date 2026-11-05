# MPAS-HT

*NSSL experimental MPAS-HTPO, 3 km*

## Why it matters

A research version of the MPAS model from NOAA's National Severe Storms Laboratory, started from the HRRR and run at 3 km. It is a different design from the HRRR and RRFS (a mesh that can change its resolution over the globe, with newer microphysics), so it is one more view of where storms may form and how they could look. This is experimental research output: it is not for forecasting and can change or stop without notice.

## Further reading

- [Model for Prediction Across Scales](https://en.wikipedia.org/wiki/Model_for_Prediction_Across_Scales) (background, Wikipedia)
- [NSSL CAMs: MPAS runs and how they are set up](https://cams.nssl.noaa.gov/) (From the people who run it)

## Where the data is read from

- [NSSL data server (FRDD/NSSL-MPAS)](https://data.nssl.noaa.gov/thredds/catalog/FRDD/NSSL-MPAS/2026/catalog.html)
- [NSSL data server, all FRDD model folders (MPAS tars are <yymmddcc>_mpasht2.tar and _mpasrn.tar; browse with an honest User-Agent, the server refuses curl's)](https://data.nssl.noaa.gov/thredds/customConfig/FRDD.xml)
