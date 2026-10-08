#!/bin/bash
# builds and runs the climatology numerics tests (no network); exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_climate_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Core) "$here/climate_test.cpp" "$root"/src/gfs/GfsClimate.cpp "$root"/src/gfs/GfsGrid.cpp "$root"/src/util/PermanentCache.cpp $(pkg-config --libs Qt6Core) -o "$out"
"$out"
