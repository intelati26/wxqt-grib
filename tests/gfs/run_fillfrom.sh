#!/bin/bash
# manual: builds and runs the ticked fills' start test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_fillfrom_test"
"$here/build_with_app.sh" "$here/fillfrom_test.cpp" "$out"
"$out"
