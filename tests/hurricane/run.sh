#!/bin/bash
# builds and runs the hurricane parser tests (ATCF rows, HDOB bulletins, gzip); exits non-zero on the first failure
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/wxqt_hurricane_test"
${CXX:-g++} -std=c++20 -O2 -Wall -Wextra -I"$root/src" "$here/hurricane_test.cpp" \
    "$root"/src/hurricane/Utility{Atcf,Hdob,EcmwfTracks,EnsembleStats,Ships,Pod,Vdm,Season,Shapefile,NhcGis,NhcText,Changes,Dropsonde}.cpp "$root"/src/util/UtilityGzip.cpp "$root"/src/util/UtilityZip.cpp -o "$out"
"$out" "$here/fixtures"
# the inland alerts reader uses Qt's JSON
alerts="${TMPDIR:-/tmp}/wxqt_alerts_test"
${CXX:-g++} -std=c++20 -fPIC -O2 -Wall -Wextra -I"$root/src" $(pkg-config --cflags Qt6Core) "$here/alerts_test.cpp" "$root"/src/hurricane/UtilityTropicalAlerts.cpp $(pkg-config --libs Qt6Core) -o "$alerts"
"$alerts" "$here/fixtures"
