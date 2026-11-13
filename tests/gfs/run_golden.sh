#!/bin/bash
# manual: builds and runs the golden-image test of the chart renderer (GOLDEN_UPDATE=1 writes the pictures again)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_golden_test"
"$here/build_with_app.sh" "$here/golden_test.cpp" "$out"
cd "$root"
"$out"
