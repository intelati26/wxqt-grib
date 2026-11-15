#!/bin/bash
# manual: builds and runs the catalogue check test (found / gone / never found / not cataloged against a made-up index)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_catalogcheck_test"
"$here/build_with_app.sh" "$here/catalogcheck_test.cpp" "$out"
"$out"
