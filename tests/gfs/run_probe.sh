#!/bin/bash
# manual: builds and runs the chart probe test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_probe_test"
"$here/build_with_app.sh" "$here/probe_test.cpp" "$out"
"$out"
