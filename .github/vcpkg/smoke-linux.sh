#!/bin/bash
# Runs the bundled GDAL tools the way wxqt does, from a clean environment with only the
# package's own gdal/ folder on PATH and library path: fails on a missing shared library
# or a driver the app needs that the minimal build left out. Linux twin of the Windows
# job's "Smoke-test bundled GDAL" step. Usage: smoke-linux.sh <scratch dir>
set -euo pipefail
t="$1"
mkdir -p "$t"

formats="$(gdalinfo --formats; ogrinfo --formats)"
for driver in GRIB GTiff PNG XYZ ENVI VRT MEM GeoJSON; do
    echo "$formats" | grep -Eq "^ *$driver\b" || { echo "GDAL driver $driver missing"; exit 1; }
done

python3 - "$t/f.asc" <<'PY'
import math, sys
rows = [" ".join(str(50 + 30 * math.sin(i / 13) * math.cos(j / 9)) for i in range(90)) for j in range(60)]
open(sys.argv[1], "w").write("\n".join(["ncols 90", "nrows 60", "xllcorner -110", "yllcorner 25", "cellsize 0.2", "NODATA_value -9999"] + rows) + "\n")
PY

run() { echo "+ $*"; "$@"; }
run gdal_translate -q -a_srs EPSG:4326 "$t/f.asc" "$t/f.tif"
run gdalwarp -q -t_srs EPSG:3857 -r cubicspline -ts 190 0 "$t/f.tif" "$t/w.tif"
run gdal_translate -q -of ENVI -ot Float32 "$t/w.tif" "$t/w.raw"
run gdaldem color-relief "$t/f.tif" "$(dirname "$0")/smoke-colors.txt" "$t/c.tif" -alpha
run gdal_translate -q -of PNG "$t/c.tif" "$t/c.png"
run gdal_contour -q -a elev -i 10 "$t/f.tif" "$t/cr.geojson"
run ogr2ogr -q -f GeoJSON "$t/cs.geojson" "$t/cr.geojson" -simplify 0.06
run ogr2ogr -q -f GeoJSON "$t/cb.geojson" "$t/cs.geojson" -dialect sqlite -sql "SELECT ST_Buffer(geometry, 0.02) AS geometry FROM contour"
run gdal_rasterize -q -burn 255 -ts 90 60 -ot Byte "$t/cb.geojson" "$t/r.tif"
# GRIB read back, incl. the complex- and JPEG2000-packed GRIB2 NCEP uses
run gdal_translate -q -of GRIB -co DATA_ENCODING=COMPLEX_PACKING "$t/f.tif" "$t/f.grb2"
run gdal_translate -q -of GRIB -co DATA_ENCODING=JPEG2000 "$t/f.tif" "$t/j.grb2"
run gdalinfo -mm "$t/f.grb2"
run gdalinfo -mm "$t/j.grb2"
run gdallocationinfo -valonly -wgs84 "$t/f.tif" -100 30
run gdalinfo -mm "$t/w.tif"
echo "bundled GDAL OK"
