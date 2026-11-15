#!/bin/bash
# manual: builds and runs the chart file test (a chart as data: the example, the checks, a drawn chart)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_userchart_test"
"$here/build_with_app.sh" "$here/userchart_test.cpp" "$out"
"$out"
