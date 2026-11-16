#!/bin/bash
# manual: builds and runs the override file test (the merge, what is refused, the registry)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_override_test"
"$here/build_with_app.sh" "$here/override_test.cpp" "$out"
"$out"
