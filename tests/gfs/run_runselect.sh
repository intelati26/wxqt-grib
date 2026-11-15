#!/bin/bash
# manual: builds and runs the chart runselect test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_runselect_test"
"$here/build_with_app.sh" "$here/runselect_test.cpp" "$out"
"$out"
