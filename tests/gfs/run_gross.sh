#!/bin/bash
# The Gross to Miserable hour score against the specification's own function (header only: no Qt)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_gross_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/gross_test.cpp" -o "$out"
"$out"
