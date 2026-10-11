#!/bin/bash
# the models written as definition files behave as they did in code: compares with tests/gfs/modelfile_golden.txt   (run_modelfile.sh --write remakes the golden)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_modelfile_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Gui) "$here/modelfile_test.cpp" "$root"/src/gfs/GfsChart.cpp "$root"/src/gfs/GfsData.cpp "$root"/src/gfs/GfsModels.cpp "$root"/src/gfs/GfsModelFile.cpp "$root"/src/gfs/GfsBuiltInModels.cpp "$root"/src/gfs/GfsPalettes.cpp "$root"/src/gfs/GfsGrid.cpp "$root"/src/spcrefs/SpcRefsCatalog.cpp "$root"/src/gfs/GfsClimate.cpp "$root"/src/util/PermanentCache.cpp "$root"/src/ui/WindBarb.cpp $(pkg-config --libs Qt6Gui) -o "$out"
if [ "$1" = "--write" ]; then
    "$out" --write "$here/modelfile_golden.txt"
else
    "$out" "$here/modelfile_golden.txt"
fi
