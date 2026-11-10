#!/bin/bash
# builds and runs the tests of the model bar, the chart selection, the hurricane id, the MAG page, the GDAL helper and the capped decoder (make first; no network)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_features_test"
"$here/build_with_app.sh" "$here/features_test.cpp" "$out"
"$out"
