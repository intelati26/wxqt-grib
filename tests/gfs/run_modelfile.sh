#!/bin/bash
# the models written as definition files behave as they did in code: compares with tests/gfs/modelfile_golden.txt   (run_modelfile.sh --write remakes the golden)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_modelfile_test"
"$here/build_with_app.sh" "$here/modelfile_test.cpp" "$out"
if [ "$1" = "--write" ]; then
    "$out" --write "$here/modelfile_golden.txt"
else
    "$out" "$here/modelfile_golden.txt"
fi
