#!/bin/bash
# manual: builds and runs the chart preset test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_preset_test"
"$here/build_with_app.sh" "$here/preset_test.cpp" "$out"
"$out"
