#!/bin/bash
# manual: builds and runs the model data cache test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_gfscache_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Core) "$here/gfscache_test.cpp" "$root"/src/gfs/GfsCache.cpp "$root"/src/gfs/GfsData.cpp "$root"/src/gfs/GfsGrid.cpp $(pkg-config --libs Qt6Core) -o "$out"
"$out"
