#!/usr/bin/env python3
"""Rebuilds every model's chart list as presets of the checks (fill field, mode, families of lines, barbs) and checks the work both ways.
Input: charts-dump.txt (WXQT_CHARTS_DUMP='*' output). Output: vocabulary.csv, presets-by-model.csv, check-report.txt."""
import re, csv, collections, itertools, sys, os
here = os.path.dirname(os.path.abspath(__file__))
rows = []
for l in open(os.path.join(here, 'charts-dump.txt')):
    f = l[6:].rstrip('\n').split('|')
    src, cat, i, lab, fill, lines, barbs, stream, ov = f[:9]
    rows.append(dict(src=src, cat=cat, id=i, lab=lab, fill=fill, lines=[t for t in lines.split(';') if t], barbs=barbs, stream=stream, ov=int(ov)))

def strip_units(t): return re.sub(r'\s*\([^)]*\)\s*$', '', t).strip()
def norm(t):
    t = re.sub(r'\d+(?:\.\d+)?', '#', strip_units(t))
    return re.sub(r'#(?:[-\u2013 ]#)+', '#', t)
def level_of(lab, i):
    m = re.search(r'(\d{3,4})\s?-\s?(\d{3,4})\s?mb', lab)
    if m: return f'{m.group(1)}-{m.group(2)} mb'
    m = re.search(r'(\d{3,4})\s?mb', lab, re.I) or re.search(r'^(\d{3,4})_', i)
    if m and 100 <= int(m.group(1)) <= 1000: return f'{m.group(1)} mb'
    if re.search(r'\b10 ?m\b', lab): return '10 m'
    if re.search(r'\b2 ?m\b', lab): return '2 m'
    return ''
def mode_of(r):
    c, i, lab = r['cat'], r['id'].lower(), r['lab'].lower()
    if 'chance' in c or c.startswith('Probability') or i.startswith(('prob', 'ppp_')) and 'prob' in i or 'probability' in lab or 'chance of' in lab: return 'Chance of a limit'
    if 'amount in the period' in c or 'percentile' in c.lower() or 'percentile' in lab: return 'Amount: expected / percentile'
    if c.startswith('Ensemble') or r['id'].startswith(('spag', 'spread', 'ens_')): return 'Ensemble: members / spread'
    if 'anom' in i: return 'Anomaly'
    if c == 'Changes and trends' or re.search(r'chng|trend|change|diff', i): return 'Change'
    return 'Value'
FAM = [('thickness', 'Thickness'), ('sea level pressure|mslp|^pressure', 'Pressure'), ('height', 'Heights'), ('humidity|dew', 'Humidity'), ('temperature', 'Temperature'), ('wind|gust|speed', 'Wind speed'),
       ('omega', 'Omega'), ('chance', 'Chance contour'), ('mean|spread', 'Ensemble mean / spread'), ('period|direction|flows', 'Waves')]
def fam(t):
    tl = t.lower()
    if re.fullmatch(r'z[a-zA-Z]\w*', t) or t in ('ECMWF ENS', 'GEFS', 'GEPS'): return 'Members'
    for pat, name in FAM:
        if re.search(pat, tl): return name
    return 'Other lines'
def spec(r):
    fams = collections.OrderedDict()
    for t in r['lines']: fams.setdefault(fam(t), []).append(strip_units(t))
    lvl = level_of(r['lab'], r['id'])
    s = dict(mode=mode_of(r), field=norm(r['fill']) if r['fill'] else '(lines only)', level=lvl, fams=tuple(sorted(fams)), barbs=bool(r['barbs']), stream=bool(r['stream']), ov=r['ov'])
    return s
for r in rows: r['spec'] = spec(r)
key = lambda i: re.sub(r'\d+(?:\.\d+)?', '#', i)
byid = collections.defaultdict(list)
for r in rows: byid[r['id']].append(r)
report = []

# --- forward: every chart has a spec that names its fill, its families and its barbs; the same id means the same spec in every model
conflict = [i for i, rs in byid.items() if len({(x['spec']['mode'], x['spec']['fams'], x['spec']['barbs'], x['spec']['stream']) for x in rs}) > 1]
report.append(f'FORWARD 1: {len(rows)} chart entries, {len(byid)} distinct ids; ids whose mode, line families or barbs differ between models: {len(conflict)}')
report += ['   ' + i + ': ' + '; '.join(sorted({f"{x['src']}={x['spec']['field']}/{','.join(x['spec']['fams'])}" for x in byid[i]}))[:160] for i in conflict[:10]]
nofill = [r for r in rows if r['spec']['field'] == '(lines only)' and not r['lines']]
report.append(f'FORWARD 2: charts with neither a fill nor lines: {len(nofill)} ' + ', '.join(sorted({r["id"] for r in nofill})[:8]))
nolevel = sorted({r['id'] for r in rows if re.match(r'^\d{3,4}_', r['id']) and not r['spec']['level']})
report.append(f'FORWARD 3: ids that start with a level but no level was read: {len(nolevel)} {nolevel[:8]}')

# --- vocabulary
vocab = collections.defaultdict(lambda: [set(), set()])
def use(kind, name, r): vocab[(kind, name)][0].add(r['id']); vocab[(kind, name)][1].add(r['src'])
for r in rows:
    s = r['spec']
    use('mode', s['mode'], r)
    if s['field'] != '(lines only)': use('fill field', s['field'], r)
    for f in s['fams']: use('line family', f, r)
    if s['barbs']: use('barbs', 'wind barbs', r)
    if s['stream']: use('streamlines', 'streamlines', r)
    if s['ov']: use('extra fills', 'bands over the fill (snow, mixed ...)', r)
    if s['level']: use('level', s['level'], r)
with open(os.path.join(here, 'vocabulary.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['kind', 'name', 'charts_using', 'models_using', 'example_ids'])
    for (k, n), (ids, ms) in sorted(vocab.items(), key=lambda x: (x[0][0], -len(x[1][0]))): w.writerow([k, n, len(ids), len(ms), ' '.join(sorted(ids)[:4])])

# --- presets per model: the chart with its level and numbers taken out (they become dropdowns)
pres = collections.defaultdict(lambda: collections.defaultdict(list))
def pkey(r):
    s = r['spec']
    return (s['mode'], s['field'] if not s['level'] else re.sub(r'\d+', '#', s['field']), ','.join(s['fams']), s['barbs'], s['stream'], s['ov'] > 0, key(r['id']))
for r in rows: pres[r['src']][pkey(r)].append(r)
out = []
for src, d in sorted(pres.items()):
    for k, rs in sorted(d.items(), key=lambda x: -len(x[1])):
        nums = []
        if len(rs) > 1:
            for col in zip(*[re.findall(r'\d+(?:\.\d+)?', x['id']) for x in rs if len(re.findall(r'\d+(?:\.\d+)?', x['id'])) == len(re.findall(r'\d+(?:\.\d+)?', rs[0]['id']))]):
                v = sorted(set(col), key=float)
                if len(v) > 1: nums.append('/'.join(v))
        out.append([src, rs[0]['lab'] if len(rs) == 1 else re.sub(r'\d+(?:\.\d+)?', '#', rs[0]['lab']), rs[0]['cat'], k[0], k[1], k[2], 'yes' if k[3] else '', 'yes' if k[4] else '', 'yes' if k[5] else '', ' | '.join(nums), len(rs), ' '.join(x['id'] for x in rs[:5]) + (' ...' if len(rs) > 5 else '')])
with open(os.path.join(here, 'presets-by-model.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['model', 'preset', 'category', 'mode', 'fill_field', 'line_families', 'barbs', 'streamlines', 'bands', 'dropdown_values', 'charts_covered', 'ids']); w.writerows(out)

# --- backward: a preset with its dropdown values must give back exactly the model's ids (no id lost, none invented)
lost = invented = 0; bad = []
for src, d in pres.items():
    have = {r['id'] for r in rows if r['src'] == src}
    made = set()
    for k, rs in d.items():
        shape = k[6]; nsl = shape.count('#')
        tuples = {tuple(re.findall(r'\d+(?:\.\d+)?', x['id'])) for x in rs}
        cols = [sorted({t[j] for t in tuples if len(t) == nsl}, key=float) for j in range(nsl)]
        for combo in itertools.product(*cols) if nsl else [()]:
            it = iter(combo); made.add(re.sub(r'#', lambda m: next(it), shape))
    lost += len(have - made); invented += len(made - have)
    if made - have: bad.append((src, sorted(made - have)[:3], len(made - have)))
report.append(f'BACKWARD 1: expanding every preset with its dropdown values: ids lost {lost}; combinations that are not charts (gaps in the dropdowns) {invented}')
report += [f'   {s}: {n} gaps, e.g. {e}' for s, e, n in sorted(bad, key=lambda x: -x[2])[:12]]
orph = [k for k, (ids, ms) in vocab.items() if not ids]
report.append(f'BACKWARD 2: vocabulary entries no chart uses: {len(orph)}')
kinds = collections.Counter(k for k, _ in vocab)
report.append('VOCABULARY: ' + ', '.join(f'{k} {n}' for k, n in sorted(kinds.items())))
tot = collections.Counter()
for src, d in pres.items(): tot[src] = len(d)
report.append(f'PRESETS: {sum(tot.values())} presets for {len(rows)} chart entries across {len(tot)} models (largest: ' + ', '.join(f'{s} {n} of {sum(1 for r in rows if r["src"]==s)}' for s, n in tot.most_common(6)) + ')')
# distinct specs once the fill field, the level and the numbers are all dropdowns
core = {(r['spec']['mode'], r['spec']['fams'], r['spec']['barbs'], r['spec']['stream'], r['spec']['ov'] > 0) for r in rows}
report.append(f'CORE: {len(core)} distinct combinations of mode + line families + barbs + streamlines + bands when the fill field and the level are dropdowns')
open(os.path.join(here, 'check-report.txt'), 'w').write('\n'.join(report) + '\n')
print('\n'.join(report))

# --- the short list: the fill field, the level and the numbers are all dropdowns, so a preset is only its mode, its line families, its barbs, its streamlines and its bands
cm = collections.defaultdict(lambda: collections.defaultdict(list))
for r in rows:
    s = r['spec']; cm[r['src']][(s['mode'], ', '.join(s['fams']), s['barbs'], s['stream'], s['ov'] > 0)].append(r)
with open(os.path.join(here, 'presets-core-by-model.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['model', 'mode', 'line_families', 'barbs', 'streamlines', 'bands', 'charts_covered', 'fill_fields_in_dropdown', 'levels_in_dropdown', 'example_ids'])
    for src, d in sorted(cm.items()):
        for k, rs in sorted(d.items(), key=lambda x: -len(x[1])):
            w.writerow([src, k[0], k[1], 'yes' if k[2] else '', 'yes' if k[3] else '', 'yes' if k[4] else '', len(rs), len({x['spec']['field'] for x in rs}), ' '.join(sorted({x['spec']['level'] for x in rs if x['spec']['level']})), ' '.join(x['id'] for x in rs[:4])])
short = {s: len(d) for s, d in cm.items()}
extra = f'SHORT LIST: {sum(short.values())} presets for {len(rows)} chart entries; per model: ' + ', '.join(f'{s} {n}/{sum(1 for r in rows if r["src"]==s)}' for s, n in sorted(short.items(), key=lambda x: -x[1])[:12])
print(extra); open(os.path.join(here, 'check-report.txt'), 'a').write(extra + '\n')
