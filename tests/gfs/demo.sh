#!/bin/bash
# manual: builds the GFS chart demo and renders one chart to a PNG   demo.sh 500_wnd_ht CONUS 24 /tmp/out.png
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_gfs_demo"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Gui) "$here/demo.cpp" "$root"/src/gfs/GfsChart.cpp "$root"/src/gfs/GfsData.cpp "$root"/src/gfs/GfsModels.cpp "$root"/src/gfs/GfsPalettes.cpp "$root"/src/gfs/GfsGrid.cpp "$root"/src/gfs/GfsClimate.cpp "$root"/src/util/PermanentCache.cpp "$root"/src/ui/WindBarb.cpp $(pkg-config --libs Qt6Gui) -o "$out"
"$out" "$@"
