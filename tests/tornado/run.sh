#!/bin/bash
# builds and runs the SPC tornado database reader tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_tornado_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/tornado_test.cpp" "$root"/src/tornado/UtilityTornado.cpp "$root"/src/obs/UtilityMetarCache.cpp -o "$out"
"$out" "$here/fixtures"
