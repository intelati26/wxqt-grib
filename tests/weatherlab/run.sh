#!/bin/bash
# builds and runs the Weather Lab (DeepMind) cyclone ensemble reader tests; exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_weatherlab_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/weatherlab_test.cpp" "$root"/src/hurricane/UtilityWeatherLab.cpp -o "$out"
"$out" "$here"
