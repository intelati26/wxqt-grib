#!/bin/bash
# manual: builds and runs the ensemble percentile test (the rule, the fan, totals, chances, the parse)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_ensemblepct_test"
"$here/build_with_app.sh" "$here/ensemblepct_test.cpp" "$out"
"$out"
