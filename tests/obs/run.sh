#!/bin/bash
# builds and runs the METAR cache / MADIS mesonet reader tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_obs_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/obs_test.cpp" "$root"/src/obs/UtilityMadis.cpp "$root"/src/obs/UtilityMetarCache.cpp "$root"/src/util/UtilityGzip.cpp -o "$out"
"$out" "$here/fixtures"
