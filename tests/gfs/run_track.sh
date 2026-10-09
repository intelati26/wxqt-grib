#!/bin/bash
# manual: builds and runs the storm track parser test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_track_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Gui) "$here/track_test.cpp" "$root"/src/gfs/GfsChart.cpp "$root"/src/gfs/GfsData.cpp "$root"/src/gfs/GfsModels.cpp "$root"/src/gfs/GfsGrid.cpp "$root"/src/gfs/GfsClimate.cpp "$root"/src/util/PermanentCache.cpp "$root"/src/ui/WindBarb.cpp $(pkg-config --libs Qt6Gui) -o "$out"
"$out"
