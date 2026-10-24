#!/bin/bash
# manual: builds and runs the model registry test
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_models_test"
"$here/build_with_app.sh" "$here/models_test.cpp" "$out"
"$out"
