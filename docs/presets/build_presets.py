#!/usr/bin/env python3
"""Rebuilds every model's chart list as presets of the checks (fill field, mode, families of lines, barbs) and checks the work both ways.
Input: charts-dump.txt (WXQT_CHARTS_DUMP='*' output). Output: vocabulary.csv, presets-by-model.csv, check-report.txt."""
import re, csv, collections, itertools, sys, os
here = os.path.dirname(os.path.abspath(__file__))
rows = []
lv = {}
for l in open(os.path.join(here, 'charts-dump.txt')):
    if l.startswith('LEVELS '):
        q = l[7:].rstrip('\n').split('|'); lv[(q[0], q[1])] = q[2:5]
for l in open(os.path.join(here, 'charts-dump.txt')):
    if not l.startswith('CHART '): continue
    f = l[6:].rstrip('\n').split('|')
    src, cat, i, lab, fill, lines, barbs, stream, ov = f[:9]
    rows.append(dict(wants=lv.get((src, i), ['', '', ''])[0], cat_at=lv.get((src, i), ['', '', ''])[1], barb_at=lv.get((src, i), ['', '', ''])[2], src=src, cat=cat, id=i, lab=lab, fill=fill, lines=[t for t in lines.split(';') if t], barbs=barbs, stream=stream, ov=int(ov)))

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

# --- the checks grid: one row per chart family, every cell taken from the vocabulary (a tick means the check is ticked, a name means the dropdown or fill it picks)
FAMS = ['Pressure', 'Heights', 'Thickness', 'Temperature', 'Humidity', 'Wind speed', 'Omega', 'Members', 'Ensemble mean / spread', 'Chance contour', 'Waves', 'Other lines']
fam_rows = collections.defaultdict(list)
for r in rows: fam_rows[key(r['id'])].append(r)
def unit_of(t):
    m = re.search(r'\(([^)]*)\)\s*$', t); return m.group(1) if m else ''
grid = []
for k, rs in sorted(fam_rows.items(), key=lambda x: (x[1][0]['cat'], x[0])):
    shapes = collections.Counter((x['spec']['mode'], x['spec']['fams'], x['spec']['barbs'], x['spec']['stream'], x['spec']['ov'] > 0) for x in rs)
    (mode, fams, barbs, stream, bands), n = shapes.most_common(1)[0]
    lead = next(x for x in rs if (x['spec']['mode'], x['spec']['fams'], x['spec']['barbs'], x['spec']['stream'], x['spec']['ov'] > 0) == (mode, fams, barbs, stream, bands))
    fields = sorted({x['spec']['field'] for x in rs if x['spec']['field'] != '(lines only)'})
    ids = sorted({x['id'] for x in rs})
    levels = sorted({x['spec']['level'] for x in rs if x['spec']['level']})
    nums = []
    if len(ids) > 1:
        cnt = len(re.findall(r'\d+(?:\.\d+)?', ids[0]))
        for col in zip(*[re.findall(r'\d+(?:\.\d+)?', i) for i in ids if len(re.findall(r'\d+(?:\.\d+)?', i)) == cnt]):
            v = sorted(set(col), key=float)
            if len(v) > 1: nums.append('/'.join(v))
    period = ''; limit = ''
    if re.search(r'(^|_)p#$|_#h(_|$)|^#h|#hour|#hr|#_?hr', k, re.I) and nums: period = nums[0]; nums = nums[1:]
    if mode in ('Chance of a limit', 'Amount: expected / percentile') and nums: limit = ' | '.join(nums); nums = []
    fillv = fields[0] if len(fields) == 1 else (f'{len(fields)} fields in one dropdown' if fields else 'none (lines only)')
    dark = 'above the limit' if mode == 'Chance of a limit' else (f'bands over the fill ({lead["ov"]})' if bands else '')
    units = sorted({unit_of(x['fill']) for x in rs if x['fill'] and unit_of(x['fill'])})
    models = sorted({x['src'] for x in rs})
    if levels: nums = [x for x in nums if not all(re.sub(r'\D', '', v) in {re.sub(r'\D', '', l) for l in ' '.join(levels).replace('-', ' ').split()} for v in x.split('/'))]
    row = [k, lead['lab'] if len(ids) == 1 else re.sub(r'\d+(?:\.\d+)?', '#', lead['lab']), lead['cat'], len(models), len(ids), mode, fillv, dark]
    row += ['yes' if f in fams else '' for f in FAMS]
    row += ['yes' if barbs else '', 'yes' if stream else '', ' | '.join(nums) if nums else '', '/'.join(levels) if len(levels) > 1 else '', period, limit, '/'.join(units), (fields[0] if len(fields) == 1 else 'one scale per field') if fields else '', 'yes' if len(shapes) > 1 else '']
    grid.append(row)
with open(os.path.join(here, 'checks-by-family.csv'), 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['family', 'example_label', 'category', 'models', 'charts', 'mode', 'fill_light (field)', 'fill_dark'] + ['lines: ' + x for x in FAMS] + ['barbs', 'streamlines', 'dropdown: other numbers', 'dropdown: level', 'dropdown: period', 'dropdown: limit / percentile', 'units', 'default scale', 'differs_by_model'])
    w.writerows(grid)
print(f'CHECKS GRID: {len(grid)} families')

# --- GFS as the standard: the preset list and the pickers behind it
gfs = []; seen = set()
for r in rows:
    if r['src'] == 'GFS' and r['id'] not in seen: seen.add(r['id']); gfs.append(r)
def gfs_layer(r):
    m = re.search(r'(\d{3,4})\s?-\s?(\d{3,4})\s?mb', r['lab'] + ' ' + ' '.join(r['lines']))
    return f'{m.group(1)}-{m.group(2)} mb' if m else ''
def lines_at(r):
    at = r['cat_at'].split(';') if r['cat_at'] else []
    out = []
    for j, t in enumerate(r['lines']):
        lvl = real_level(at[j]) if j < len(at) else ''
        f = fam(t)
        out.append(f + ' ' + (strip_units(t) if f in ('Thickness', 'Temperature') else (lvl or 'derived field')) + ('' if lvl or f == 'Thickness' else '') + (' (checked)' if lvl else ''))
    return out
def real_level(t):
    m = re.search(r'@(\d+) mb', t)
    if m: return m.group(1) + ' mb'
    if '10 m above' in t: return '10 m'
    if 'mean sea level' in t: return 'MSLP'
    if '2 m above' in t: return '2 m'
    return ''
def lv_label(r): return r['spec']['level']
def barbs_at(r):
    if not r['barbs']: return ''
    got = [real_level(t) for t in r['barb_at'].split(';') if t]
    got = [g for g in got if g]
    if got: return ' + '.join(got) + ' (checked)'
    lv = r['spec']['level']
    if lv and not lv.endswith('2 m'): return lv
    if re.search(r'10 ?m', r['lab'] + r['id']): return '10 m'
    return 'derived wind, not in the files (' + ('label says ' + lv_label(r) if lv_label(r) else 'no level in the label') + ')'
gp = collections.defaultdict(list)
for r in gfs:
    s = r['spec']; gp[(s['mode'], re.sub(r'\d+(?:\.\d+)?', '#', s['field']), s['fams'], s['barbs'], s['stream'], s['ov'] > 0, re.sub(r'^\d+_\d+_thick', 'THICK', re.sub(r'^\d+_\d+_thick_type', 'THICK_type', key(r['id']))) if 'thick' in r['id'] else key(r['id']))].append(r)
def dd(rs):
    k0 = key(rs[0]['id']); out = []
    if len(rs) < 2: return out
    nums = []
    c0 = len(re.findall(r'\d+(?:\.\d+)?', rs[0]['id']))
    for col in zip(*[re.findall(r'\d+(?:\.\d+)?', x['id']) for x in rs if len(re.findall(r'\d+(?:\.\d+)?', x['id'])) == c0]):
        v = sorted(set(col), key=float)
        if len(v) > 1: nums.append('/'.join(v))
    if 'thick' in k0: return ['Layer: ' + ' / '.join(sorted({gfs_layer(x) for x in rs if gfs_layer(x)}))]
    if re.search(r'p#$', k0): return ['Period (h): ' + nums[0].replace('01', '1').replace('03', '3').replace('06', '6')]
    return [('Level (mb): ' if any(r['spec']['level'] for r in rs) else 'Number: ') + n for n in nums]
with open(os.path.join(here, 'gfs-presets.csv'), 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['preset', 'category', 'mode', 'fill (light)', 'fill (dark / highlight)', 'value dropdown(s)', 'contours', 'barbs', 'streamlines', 'type bands', 'charts covered', 'chart ids'])
    for k, rs in sorted(gp.items(), key=lambda x: (x[1][0]['cat'], -len(x[1]))):
        r0 = rs[0]
        lab = r0['lab'] if len(rs) == 1 else re.sub(r'\d+(?:\.\d+)?', '#', r0['lab'])
        lv = sorted({x['spec']['level'] for x in rs if x['spec']['level']})
        w.writerow([lab, r0['cat'], k[0], k[1] if k[1] != '(lines only)' else '', 'bands: rain / snow / mixed' if k[5] else '', ' | '.join(dd(rs)),
                    ' + '.join(lines_at(r0)), ('barbs @ ' + barbs_at(r0)) if r0['barbs'] else '', 'yes' if k[4] else '', 'yes' if k[5] else '', len(rs), ' '.join(x['id'] for x in rs)])
ent = collections.defaultdict(list)
def add(p, e, r): ent[(p, e)].append(r['id'])
for r in gfs:
    s = r['spec']
    add('Mode', s['mode'], r)
    if s['field'] != '(lines only)': add('Fill field', s['field'], r)
    if s['level']: add('Level / layer', s['level'], r)
    if re.search(r'hour', s['field'], re.I): add('Period', re.sub(r'\D', '', re.search(r'p(\d+)$', r['id']).group(1)) + ' h' if re.search(r'p(\d+)$', r['id']) else 'total / since start', r)
    for t in r['lines']: add('Contours: ' + fam(t), strip_units(t), r)
    if r['barbs']: add('Barbs (levels stack)', barbs_at(r), r)
    if s['stream']: add('Streamlines', s['level'] or 'chart level', r)
    if s['ov']: add('Type bands', 'rain / snow / mixed', r)
with open(os.path.join(here, 'gfs-pickers.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['picker', 'entry', 'GFS charts using it', 'example ids'])
    for (p, e), ids in sorted(ent.items(), key=lambda x: (x[0][0], -len(x[1]))): w.writerow([p, e, len(ids), ' '.join(ids[:4])])
print(f'GFS: {len(gfs)} charts, {len(gp)} presets, {len(ent)} picker entries')
