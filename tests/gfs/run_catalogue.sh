#!/bin/bash
# manual: dumps every chart of every model from the built program (build/release/wxqt) and compares it with the saved list in docs/presets/charts-dump.txt (id, group, label, fill and line titles,
# the records and levels each chart reads). Moving the chart definitions between files must not change one line of it. After a change that is meant to (a new chart), save the list again:
#   tests/gfs/run_catalogue.sh save
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
cd "$root"
now="${TMPDIR:-/tmp}/wxqt_catalogue_now.txt"
(WXQT_CHARTS_DUMP='*' QT_QPA_PLATFORM=offscreen timeout 60 build/release/wxqt 2>&1 >/dev/null | grep -E "^(CHART|LEVELS|CHECK)" | sort > "$now") || true
count=$(wc -l < "$now")
if [ "$1" = "save" ]; then
    cp "$now" docs/presets/charts-dump.txt
    echo "saved $count lines"
    exit 0
fi
grep -E "^(CHART|LEVELS|CHECK)" docs/presets/charts-dump.txt | sort > "$now.saved"
if diff -q "$now.saved" "$now" > /dev/null && [ "$count" -gt 1000 ]; then
    echo "OK ($count lines the same)"
else
    echo "FAILED: the charts differ from docs/presets/charts-dump.txt ($count lines now)"
    diff "$now.saved" "$now" | head -20
    exit 1
fi
