# Credits

wxqt is licensed under the GNU GPLv3. It stands on the work of the projects and data providers below; the licences named are theirs. Where a library is only
called as a separate program, or only used to check our own code, that is said.

## Libraries and tools
- **Qt** (LGPLv3 / GPL): the user interface, networking, image drawing. https://www.qt.io
- **GDAL** (MIT, Copyright (c) Frank Warmerdam, Even Rouault and contributors): decodes GRIB2 and NetCDF, warps grids and reads Zarr; called as `gdal_translate`, `gdalwarp`
  and `gdalinfo` programs. https://gdal.org
- **PROJ** (MIT): map projections, through GDAL. https://proj.org
- **ECMWF ecCodes** (Apache License 2.0, Copyright ECMWF): the reference GRIB / BUFR decoder. The values the BUFR track reader's tests expect were read with its tools
  (`tests/hurricane/`), and the open-data GRIB reader planned for ECMWF's models will use its field names and index. https://confluence.ecmwf.int/display/ECC
- **SHARPpy** (BSD): the sounding algorithms ported to C++, see `sharppy-notice.md`.
- **zstd** (BSD / GPLv2, Meta): vendored for the Zarr reader, see `zstd-LICENSE.txt`.

## Data
- **NOAA / NWS / NCEP / NHC / SPC / WPC / OPC / NESDIS / NSSL / MRMS**: US government works. GFS, GEFS, NBM, HREF, RRFS, REFS, HAFS, GFS-Wave, AIGFS and the rest of the
  model guidance come from NOAA's open data (NOMADS and the AWS Open Data buckets).
- **U.S. Drought Monitor** (produced jointly by the National Drought Mitigation Center at the University of Nebraska-Lincoln, the USDA and NOAA; droughtmonitor.unl.edu): the weekly maps, the change maps and the area statistics on the drought screen.
- **NOAA Climate Prediction Center**: the precipitation totals and departures, the drought outlooks, soil moisture and the standardized precipitation index on the drought screen.
- **ECMWF open data** (CC BY 4.0): "Contains ECMWF open data", see `ecmwf-notice.md`.
- **Google DeepMind Weather Lab** cyclone ensembles: used under the terms on their download pages (experimental; not for real world use).
- **CIRA / RAMMB** (Colorado State), **JTWC**, **JMA**, **NOAA PSL** (the NCEP / NCAR reanalysis climatology for the anomaly charts), **Natural Earth** (coastlines,
  public domain, see `naturalearth-notice.md`).
- The colour scales of the "model guidance site" look were read from the legends of NCEP's Model Analyses and Guidance pages (https://mag.ncep.noaa.gov).

## Ideas
- The time bar, the grouped model menu and the compare modes follow the layout of Pivotal Weather's model pages; the code is our own.
- The app began as the Kotlin / C++ weather app of Joshua Tee, see the README.
