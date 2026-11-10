#!/bin/bash
# manual: builds from the program's own objects (make first) and runs gfscache_test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_gfscache_test"
"$here/build_with_app.sh" "$here/gfscache_test.cpp" "$out"
"$out"
