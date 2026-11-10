#!/bin/bash
# manual: builds from the program's own objects (make first) and runs track_test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_track_test"
"$here/build_with_app.sh" "$here/track_test.cpp" "$out"
"$out"
