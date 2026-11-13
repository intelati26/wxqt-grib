#!/bin/bash
# manual: builds and runs the checks picker's kinds test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_checkspicker_test"
"$here/build_with_app.sh" "$here/checkspicker_test.cpp" "$out"
QT_QPA_PLATFORM=offscreen "$out"
