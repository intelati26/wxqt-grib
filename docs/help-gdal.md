# GDAL and NetCDF files

GDAL is the library that decodes the model files (GRIB, NetCDF, HDF5). Every model chart needs it. Settings > **GDAL**.

## Where the program finds it

It looks in a folder called `gdal` next to the program (the Windows package has one), then on the `PATH`. To use another
install, give the **folder that holds `gdalwarp`**: on Windows the `bin` folder of OSGeo4W (`C:\OSGeo4W\bin`), of QGIS
(`...\QGIS 3.xx\bin`) or of a conda environment (`...\Library\bin`). The folders of GDAL's and PROJ's tables are taken from beside
it (`share\gdal`, `share\proj`). **Choose the folder...** picks it; **Use the default** goes back; empty is what the program finds
by itself. If there is no `gdalwarp` in the folder you gave, the default is used and the tab says so. On Linux, install the
`gdal` package.

**Check for a newer GDAL** asks GitHub (`api.github.com`, OSGeo/gdal) for the newest release, only when you press it, and
shows the version you have beside it. **Keeping it current is worth the few minutes:** every release fixes bugs and security
problems and widens what GDAL can read (the GRIB2 and NetCDF files the models are made of); the model files change faster than
the old libraries learn them. The Windows portable package and the Linux AppImage are built with the project's choice of GDAL
release (the newest it has been tried with; the project checks GitHub for a newer one every day and opens an issue when there is
one), so the newest package carries it. Your own install (OSGeo4W, QGIS, conda, your distribution's `gdal` package) is updated the
way you updated it, and then chosen here.

## NetCDF files read whole (Windows and macOS)

GDAL's NetCDF driver can read a *part* of a remote NetCDF-4 file only on Linux. Elsewhere the program first reads the variable it
needs by byte ranges itself; if that cannot be done it downloads the whole file, once, and keeps it in the cache. A few sources
have no other way, and their files are large: GEOS-FP's hourly surface file is 48 MB, its aerosol file 260 MB, its 3 hourly
pressure-level file about 480 MB, SPC's mesoanalysis about 520 MB an hour and the RRFSD severe fields about 3.8 GB an hour.
"Up to 100 MB (the default)" does not download files over that size; "None" never downloads a whole file. Raise it only if you
want those sources and have the disk and the bandwidth.
