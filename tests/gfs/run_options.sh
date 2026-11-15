#!/bin/bash
# manual: builds and runs the chart options test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_options_test"
"$here/build_with_app.sh" "$here/options_test.cpp" "$out"
"$out"
