#!/bin/bash
# builds and runs the SWPC (space weather) reader tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_space_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Core) "$here/space_test.cpp" "$root"/src/space/UtilitySpace.cpp "$root"/src/obs/UtilityMetarCache.cpp $(pkg-config --libs Qt6Core) -o "$out"
"$out" "$here/fixtures"
