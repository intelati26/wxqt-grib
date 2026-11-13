#!/bin/bash
# manual: builds and runs the ticked lines' style test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_linestyle_test"
"$here/build_with_app.sh" "$here/linestyle_test.cpp" "$out"
"$out"
