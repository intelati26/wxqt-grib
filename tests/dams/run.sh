#!/bin/bash
# builds and runs the dam registry / CWMS reader tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_dams_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/dams_test.cpp" "$root"/src/dams/UtilityDams.cpp -o "$out"
"$out" "$here/fixtures"
