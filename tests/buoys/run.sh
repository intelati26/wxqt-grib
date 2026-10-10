#!/bin/bash
# builds and runs the NDBC buoy parser tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_buoys_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/buoys_test.cpp" "$root"/src/buoys/UtilityBuoys.cpp "$root"/src/util/UtilityDate.cpp -o "$out"
"$out" "$here/fixtures"
