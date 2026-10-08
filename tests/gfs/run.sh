#!/bin/bash
# builds and runs the GFS chart numerics tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_gfs_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/gfs_test.cpp" "$root"/src/gfs/GfsGrid.cpp -o "$out"
"$out"
