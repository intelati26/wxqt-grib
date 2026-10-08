#!/bin/bash
# builds and runs the ISO date writer tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_date_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/date_test.cpp" "$root"/src/util/UtilityDate.cpp -o "$out"
"$out"
