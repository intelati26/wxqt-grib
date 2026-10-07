#!/bin/bash
# builds and runs the hurricane parser tests (ATCF rows, HDOB bulletins, gzip); exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_hurricane_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/hurricane_test.cpp" \
    "$root"/src/hurricane/Utility{Atcf,Hdob,EcmwfTracks,EnsembleStats,Ships,Pod,Vdm}.cpp "$root"/src/util/UtilityGzip.cpp -o "$out"
"$out" "$here/fixtures"
