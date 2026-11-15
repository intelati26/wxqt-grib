#!/bin/bash
# manual: builds and runs the chart barbgrid test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_barbgrid_test"
"$here/build_with_app.sh" "$here/barbgrid_test.cpp" "$out"
"$out"
