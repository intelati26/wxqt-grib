#!/bin/bash
# manual: builds and runs the server status test (OK, Late, Failing, Down, Retired from checks at made-up times)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_serverstatus_test"
"$here/build_with_app.sh" "$here/serverstatus_test.cpp" "$out"
"$out"
