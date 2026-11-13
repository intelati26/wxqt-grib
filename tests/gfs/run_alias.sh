#!/bin/bash
# manual: builds and runs the chart alias table test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_alias_test"
"$here/build_with_app.sh" "$here/alias_test.cpp" "$out"
"$out"
