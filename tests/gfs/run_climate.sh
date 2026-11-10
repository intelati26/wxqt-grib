#!/bin/bash
# manual: builds from the program's own objects (make first) and runs climate_test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_climate_test"
"$here/build_with_app.sh" "$here/climate_test.cpp" "$out"
"$out"
