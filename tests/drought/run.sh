#!/bin/bash
# builds and runs the drought shape tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_drought_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Gui) "$here/drought_test.cpp" "$root"/src/drought/UtilityDrought.cpp "$root"/src/drought/DroughtHistory.cpp "$root"/src/util/PermanentCache.cpp "$root"/src/hurricane/UtilityShapefile.cpp \
    "$root"/src/util/UtilityZip.cpp "$root"/src/util/UtilityGzip.cpp $(pkg-config --libs Qt6Gui) -o "$out"
"$out" "$here/fixtures"
