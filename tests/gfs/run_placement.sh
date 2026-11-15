#!/bin/bash
# manual: builds and runs the chart placement test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_placement_test"
"$here/build_with_app.sh" "$here/placement_test.cpp" "$out"
"$out"
