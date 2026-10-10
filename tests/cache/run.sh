#!/bin/bash
# builds and runs the permanent cache tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_cache_test_bin"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Core) "$here/cache_test.cpp" "$root"/src/util/PermanentCache.cpp $(pkg-config --libs Qt6Core) -o "$out"
"$out"
