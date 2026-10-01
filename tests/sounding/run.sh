#!/bin/bash
# builds and runs the sounding regression test; exits non-zero on any out-of-tolerance value
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_sounding_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/sounding_test.cpp" \
    "$root"/src/sounding/Sounding{Thermo,Profile,Parcel,Indices,Analysis}.cpp -o "$out"
"$out" "$here"/fixtures/*.txt
