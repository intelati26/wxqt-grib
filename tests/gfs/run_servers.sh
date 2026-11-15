#!/bin/bash
# manual: builds and runs the servers-by-name test (the file's checks, the layers, {@name} places, a model's urls through the table)
set -e
here="$(cd "$(dirname "$0")" && pwd)"
out="${TMPDIR:-/tmp}/wxqt_servers_test"
"$here/build_with_app.sh" "$here/servers_test.cpp" "$out"
"$out"
