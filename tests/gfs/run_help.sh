#!/bin/bash
# the Help window's guides: every file the qrc names exists, every guide the tree names (HelpViewer.cpp: "guide:<name>") is in the qrc, and a link from one guide to another (<name>.md) resolves to a bundled guide
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
python3 - "$root" <<'PY'
import os, re, sys
root = sys.argv[1]
qrc = open(os.path.join(root, 'help', 'help.qrc')).read()
fails = 0
bundled = {}
for alias, path in re.findall(r'<file alias="([^"]+)">([^<]+)</file>', qrc):
    full = os.path.normpath(os.path.join(root, 'help', path))
    if not os.path.isfile(full):
        print('FAIL: the qrc names a file that is not there:', path); fails += 1
    bundled[alias] = full
tree = open(os.path.join(root, 'src', 'misc', 'HelpViewer.cpp')).read()
guides = set(re.findall(r'"guide:([a-z0-9-]+)"', tree))
for g in sorted(guides):
    if 'guides/%s.md' % g not in bundled:
        print('FAIL: the Help tree names guide:%s and the qrc has no guides/%s.md' % (g, g)); fails += 1
names = {a[len('guides/'):-3] for a in bundled if a.startswith('guides/')}
for n in sorted(names):
    text = open(bundled['guides/%s.md' % n]).read()
    if len(text) < 500:
        print('FAIL: guide %s is nearly empty' % n); fails += 1
    for link in re.findall(r'\]\(([A-Za-z0-9._-]+\.md)\)', text):
        if link[:-3] not in names:
            print('FAIL: guide %s links to %s, which is not a bundled guide' % (n, link)); fails += 1
print('guides: %d, tree names: %d' % (len(names), len(guides)))
print('all help checks passed' if fails == 0 else '%d failures' % fails)
sys.exit(1 if fails else 0)
PY
