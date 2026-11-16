#!/bin/bash
# manual: builds and runs the NBM text bulletin test (the parse of real KOKC blocks in tests/gfs/nbm/, the merge, the finder on a stand-in file). With the real bulletin files, the finder is tried on EVERY
# station of each: NBM_FILE_NBS=... NBM_FILE_NBE=... NBM_FILE_NBX=... tests/gfs/run_nbmtext.sh (the files are blend_nbstx.t06z and so on from https://noaa-nbm-grib2-pds.s3.amazonaws.com/blend.<day>/<HH>/text/)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_nbmtext_test"
"$here/build_with_app.sh" "$here/nbmtext_test.cpp" "$out"
"$out" "$here/nbm"
