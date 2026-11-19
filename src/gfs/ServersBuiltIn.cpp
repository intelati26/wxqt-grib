// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The built-in server table: the text of servers/servers.json of the repository (a test compares the two). Edit the file and run python3 servers/make_builtin.py.
#include "gfs/Servers.h"

const std::string& Servers::builtInJson() {
    static const std::string text = R"JSON({
  "version": 1,
  "updated": "2026-11-16",
  "servers": {
    "rrfs-ops": {
      "bases": [
        "https://noaa-rrfs-ops-pds.s3.amazonaws.com",
        "https://noaa-rrfs-ops-pds.s3.us-east-1.amazonaws.com"
      ],
      "note": "NOAA RRFS, REFS and the RRFS ensemble: the operational open-data bucket"
    },
    "gfs-aws": {
      "bases": [
        "https://noaa-gfs-bdp-pds.s3.amazonaws.com",
        "https://noaa-gfs-bdp-pds.s3.us-east-1.amazonaws.com"
      ],
      "note": "NOAA GFS: the open-data bucket"
    },
    "ecmwf-gcs": {
      "bases": [
        "https://storage.googleapis.com/ecmwf-open-data"
      ],
      "note": "ECMWF open data (IFS, IFS-ENS, AIFS) replicated on Google's storage"
    },
    "geos-nccs": {
      "bases": [
        "https://portal.nccs.nasa.gov/datashare/gmao/geos-fp/forecast"
      ],
      "note": "NASA GMAO GEOS-FP forecast files"
    },
    "nbm-aws": {
      "bases": [
        "https://noaa-nbm-grib2-pds.s3.amazonaws.com"
      ],
      "note": "NOAA National Blend of Models: the open-data bucket (grids and the text bulletins)"
    },
    "gefs-aws": {
      "bases": [
        "https://noaa-gefs-pds.s3.amazonaws.com"
      ],
      "note": "NOAA GEFS: the open-data bucket"
    },
    "cfs-aws": {
      "bases": [
        "https://noaa-cfs-pds.s3.amazonaws.com"
      ],
      "note": "NOAA CFS and its reanalysis: the open-data bucket"
    },
    "hrrr-aws": {
      "bases": [
        "https://noaa-hrrr-bdp-pds.s3.amazonaws.com"
      ],
      "note": "NOAA HRRR (CONUS and Alaska): the open-data bucket"
    },
    "aigfs-aws": {
      "bases": [
        "https://noaa-nws-graphcastgfs-pds.s3.amazonaws.com"
      ],
      "note": "NOAA AIGFS (the AI model): the open-data bucket"
    },
    "mrms-aws": {
      "bases": [
        "https://noaa-mrms-pds.s3.amazonaws.com"
      ],
      "note": "NOAA MRMS radar mosaics: the open-data bucket"
    },
    "rtma-aws": {
      "bases": [
        "https://noaa-rtma-pds.s3.amazonaws.com"
      ],
      "note": "NOAA RTMA (real-time analysis): the open-data bucket"
    },
    "urma-aws": {
      "bases": [
        "https://noaa-urma-pds.s3.amazonaws.com"
      ],
      "note": "NOAA URMA (unrestricted analysis): the open-data bucket"
    },
    "era5-ncar": {
      "bases": [
        "https://nsf-ncar-era5.s3.amazonaws.com"
      ],
      "note": "ERA5 reanalysis as NetCDF at NSF NCAR"
    },
    "nomads": {
      "bases": [
        "https://nomads.ncep.noaa.gov/pub/data/nccf/com"
      ],
      "note": "NOAA's operational model server (HAFS, HREF, SPC post-processing ...)"
    },
    "naqfc-aws": {
      "bases": [
        "https://noaa-nws-naqfc-pds.s3.amazonaws.com"
      ],
      "note": "NOAA's air quality forecast model files (AQMv7, CMAQ) in the open data bucket"
    },
    "ncei-models": {
      "bases": [
        "https://www.ncei.noaa.gov/oa/prod-model"
      ],
      "note": "NOAA NCEI's model archive (GFS analysis, NARR, NAM and RAP analyses)"
    },
    "dwd-opendata": {
      "bases": [
        "https://opendata.dwd.de/weather/nwp"
      ],
      "note": "The German weather service's open data (ICON-D2, ICON-EU)"
    },
    "msc-datamart": {
      "bases": [
        "https://dd.weather.gc.ca"
      ],
      "note": "Environment Canada's data mart (HRDPS, GEPS)"
    },
    "openmeteo-s3": {
      "bases": [
        "https://openmeteo.s3.amazonaws.com/data_spatial"
      ],
      "note": "Open-Meteo's model files (.om), a bucket of grids"
    },
    "openmeteo-seasonal": {
      "bases": [
        "https://seasonal-api.open-meteo.com"
      ],
      "note": "Open-Meteo's seasonal service (EC46, SEAS5)"
    },
    "nssl-thredds": {
      "bases": [
        "https://data.nssl.noaa.gov/thredds/fileServer/FRDD"
      ],
      "note": "NSSL's THREDDS file server (the MPAS runs)"
    },
    "spc-refs": {
      "bases": [
        "https://www.spc.noaa.gov/exper/refs/data"
      ],
      "note": "SPC's REFS store (Zarr)"
    },
    "wpc-ppp": {
      "bases": [
        "https://ftp-wpc.ncep.noaa.gov/prob_precip_portal"
      ],
      "note": "WPC's probabilistic precipitation portal"
    },
    "woof-nam": {
      "bases": [
        "https://woof-nam.recastsystems.com"
      ],
      "note": "WOOF-NAM, a hosted NAM 12 km run"
    }
  }
}
)JSON";
    return text;
}
