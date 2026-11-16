#!/bin/bash
# manual: builds and runs the model bias test (errors, days, the parse)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_modelbias_test"
"$here/build_with_app.sh" "$here/modelbias_test.cpp" "$out"
"$out"
