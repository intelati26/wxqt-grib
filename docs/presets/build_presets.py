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

# --- the Chance family: every chance chart parsed into product, window, limit(s) and kind
PROD = [('blizzard', 'Blizzard'), ('wind > .* rh|rh <', 'Fire weather: wind and RH'), ('fosberg', 'Fosberg index'), ('stp', 'Significant tornado parameter'),
        ('tornado', 'Severe: tornado'), ('hail', 'Severe: hail'), ('any severe|severe', 'Severe: any'), ('wind probability', 'Severe: wind'), ('thunder', 'Thunderstorm'),
        ('flight rules|ifr|vfr', 'Flight category'), ('visibility', 'Visibility'), ('ceiling', 'Ceiling'), ('echo top', 'Echo top'), ('reflectivity|refc|refd|dbz', 'Reflectivity'),
        ('helicity|\\buh\\b|uphl|hlcy', 'Updraft helicity'), ('updraft speed', 'Updraft speed'), ('cape', 'CAPE'), ('shear', 'Shear'), ('precipitable', 'Precipitable water'),
        ('2 ?m temperature|2m temp|t2m', '2 m temperature'), ('dew', '2 m dewpoint'), ('10 ?m wind|wind speed|10m_wspd', '10 m wind speed'),
        ('freezing rain|frzr|ice pellet|\\bice\\b|sleet', 'Ice (freezing rain, sleet)'), ('snow', 'Snow'), ('rain|qpf|precip', 'Rain / total precipitation')]
def chance_parse(r):
    lab = r['lab']; ll = lab.lower(); i = r['id'].lower()
    if re.search(r'probability matched mean|lpmm|pmm', ll + ' ' + i): return dict(fit='not a chance (a probability-matched mean amount)', product='Rain / total precipitation' if 'precip' in ll else 'Reflectivity')
    prod = next((n for p, n in PROD if re.search(p, ll + ' ' + i)), 'other')
    m = re.search(r'(\d+)[- ]?(?:hr|h|hour)s?\b', ll) or re.search(r'_(\d+)h', i)
    window = (m.group(1) + ' h') if m else ('rest of the convective day' if 'rest of the convective day' in ll or '_full' in i else ('since the start of the run' if 'start of the run' in ll or 'total' in i else ''))
    if 'last hour' in ll: window = '1 h'
    kind = ('neighborhood' if 'neighborhood' in ll or 'nmep' in i else 'calibrated' if re.search(r'calibrated|gefs-mlp|refsct|gefsct', ll) else 'pooled' if 'pooled' in ll else 'agreement scale' if 'agreement scale' in ll else "members' chance" if "members' chance" in ll else 'ensemble')
    txt = re.sub(r'\d+[- ]?(?:hr|h|hour)s?\b', ' ', ll)
    txt = re.sub(r'\d+(?:-\d+)? ?-?km\b', ' ', txt)
    txt = re.sub(r'last \d+ runs', ' ', txt)
    lims = []
    for d, n, u in re.findall(r'(>|<|over|under|between|more than|less than|and|of)?\s*(-?\d[\d,]*\.?\d*)\s*(inch(?:es)?|in|miles?|mi|kt|mph|dbz|m/s|m2/s2|j/kg|feet|ft|percentile|%|f|c)?(?=\W|$)', txt):
        d = d.strip()
        if u or d in ('>', '<', 'over', 'under', 'between', 'more than', 'less than') or (d == 'and' and lims and lims[-1][0] == 'between') or (d == 'of' and re.search(re.escape(n) + r' or (more|less)', txt)):
            lims.append((d if d != 'of' else '>=', n, u))
    if 'freezing' in ll and 'rain' not in ll and 'freezing' in i: lims.append(('<=', '0', 'C'))
    lim_texts = [' '.join(x for x in l if x) for l in lims]
    nl = len(lims) - (1 if lims and lims[0][0] == 'between' else 0)
    fit = 'fits (product, window, one limit, kind)'
    if re.search(r'blizzard|low instrument flight rules and', ll) or (nl > 1 and not re.search(r'wind > .* and rh', ll)) or ('between' in ll): fit = 'two or more limits'
    if re.search(r'wind > .* and rh', ll): fit = 'fits (product, window, one limit, kind)'   # the fire weather pairs: a second limit row, only for this group
    elif re.search(r'flight rules|ifr|vfr', ll) or kind == 'calibrated' or not lims: fit = 'no limit (fixed definition)'
    if kind == "members' chance": fit = 'not a chance family (chance contour over a base chart)'
    return dict(fit=fit, product=prod, window=window, kind=kind, limits=' & '.join(lim_texts[:3]))
chance_rows = [r for r in rows if r['spec']['mode'] == 'Chance of a limit']
with open(os.path.join(here, 'chance-charts.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['model', 'chart id', 'label', 'product', 'window', 'limit(s) read from the label', 'kind', 'fit'])
    cnt = collections.Counter(); byprod = collections.defaultdict(collections.Counter)
    for r in chance_rows:
        p = chance_parse(r); cnt[p['fit']] += 1; byprod[p['product']][p['fit'][:4]] += 1
        w.writerow([r['src'], r['id'], r['lab'], p['product'], p.get('window', ''), p.get('limits', ''), p.get('kind', ''), p['fit']])
print('CHANCE:', len(chance_rows), 'charts;', dict(cnt))
print('  products:', {k: sum(v.values()) for k, v in sorted(byprod.items(), key=lambda x: -sum(x[1].values()))})

# --- the proposed families, one row each: what it draws, the dropdowns it needs, how many charts and models use it
def fam_of(r):
    s = r['spec']
    if s['mode'] == 'Chance of a limit': return 'Chance'
    if s['mode'] == 'Amount: expected / percentile': return 'Amount (expected, percentile)'
    if s['mode'] == 'Ensemble: members / spread': return 'Ensemble (members, spread)'
    if s['mode'] == 'Anomaly': return 'Anomaly'
    if s['mode'] == 'Change': return 'Change'
    if re.search(r'precipitation|precip rate|precip', re.sub(r'precipitable', '', s['field'], flags=re.I), re.I) and not r['id'].startswith(('prob', 'ens_')): return 'Precipitation (period)'
    return 'Value (field at a level)'
FAMDEF = {
 'Value (field at a level)': ('One field drawn as a fill, with the overlays ticked', 'Field | Level or layer | (Units)', 'The default for deterministic models; 200/250/300 mb winds start without barbs'),
 'Precipitation (period)': ('Accumulated precipitation over a period, with MSLP and thickness or the type bands', 'Period | Shown as (amount, type, rate) | Units', 'Period list is per model (GFS 1-60 h, NBM 1/6/12 h, IFS 3-60 h)'),
 'Chance': ('Chance of the product exceeding a limit, with a highlight above a chosen chance', 'Product | Window | Limit (with direction) | Kind (ensemble, neighborhood, pooled)', '17 charts need two limits; 35 have a fixed definition; 7 are probability-matched means and belong to Amount'),
 'Amount (expected, percentile)': ('An amount from an ensemble: the expected value or a percentile', 'Product | Window | Statistic (expected, 5th ... 95th percentile)', 'WPC percentiles and the extreme forecast index charts (put here by their names, not checked one by one)'),
 'Ensemble (members, spread)': ('The ensemble itself: mean, spread, or the members as contours (spaghetti)', 'Field | Level | Statistic (mean, spread, member, all members) | Contour value (for the spaghetti)', 'The spaghetti contour value is the dependent list (a height value that exists for that level)'),
 'Anomaly': ('The field minus its climate', 'Field | Level | (Climate period)', 'Few: GFS 500 and 700 mb heights and MSLP, the CFS and the EC46'),
 'Change': ('The field now minus the field at an earlier time', 'Field | Level | Since (F00, 6, 12, 24, 48 h earlier)', 'Few per model; GFS has MSLP, 500 mb height and snow depth'),
}
fc = collections.defaultdict(lambda: [0, set(), set(), []])
for r in rows:
    k = fam_of(r); fc[k][0] += 1; fc[k][1].add(r['src']); fc[k][2].add(r['id']); fc[k][3].append(r['id'])
with open(os.path.join(here, 'families-proposed.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['family', 'what it draws', 'dropdowns (in order)', 'chart entries', 'distinct ids', 'models', 'example ids', 'where it does not fit'])
    for k in FAMDEF:
        n, ms, ids, ex = fc[k]
        w.writerow([k, FAMDEF[k][0], FAMDEF[k][1], n, len(ids), len(ms), ' '.join(sorted(ids)[:4]), FAMDEF[k][2]])
    ov = [('Contours: pressure (MSLP)', 'Pressure'), ('Contours: heights', 'Heights'), ('Contours: thickness', 'Thickness'), ('Contours: temperature', 'Temperature'), ('Contours: omega', 'Omega'), ('Contours: ensemble members / mean / spread', 'Members'), ('Contours: chance (line form of Chance)', 'Chance contour'), ('Contours: other (CAPE/CIN, ship, CFS)', 'Other lines'), ('Contours: waves', 'Waves')]
    for name, fm in ov:
        ids = {r['id'] for r in rows if fm in r['spec']['fams'] or (fm == 'Ensemble mean / spread' and 'Ensemble mean / spread' in r['spec']['fams'])}
        if fm == 'Members': ids |= {r['id'] for r in rows if 'Ensemble mean / spread' in r['spec']['fams']}
        n = sum(1 for r in rows if r['id'] in ids); ms = {r['src'] for r in rows if r['id'] in ids}
        w.writerow([name, 'An overlay family: its own levels, line style and spacing', 'Level (when it has levels) | Style (swatch)', n, len(ids), len(ms), ' '.join(sorted(ids)[:4]), {'Other lines': 'Not a family: each needs its own line family or stays a one-off chart', 'Chance contour': 'Folds into the Chance highlight', 'Members': 'Needs a family that draws many members at once (up to 51)'}.get(fm, '')])
    for name, cond, dd in [('Wind barbs (levels stack)', lambda r: r['spec']['barbs'], 'Level toggles, one colour each'), ('Streamlines', lambda r: r['spec']['stream'], 'Level'), ('Type bands (rain / snow / mixed)', lambda r: r['spec']['ov'] > 0, 'None (an option on the fill)')]:
        rs = [r for r in rows if cond(r)]
        w.writerow([name, 'An overlay', dd, len(rs), len({r['id'] for r in rs}), len({r['src'] for r in rs}), ' '.join(sorted({r['id'] for r in rs})[:4]), ''])
print('FAMILIES: ' + ', '.join(f'{k} {fc[k][0]}' for k in FAMDEF))

# --- model by model: every chart entry goes to its family and its axes are read; whatever cannot be read is listed
def axes(r):
    """returns (family, ok, note) for one chart entry"""
    fm = fam_of(r); lab = r['lab'].lower(); i = r['id'].lower(); s = r['spec']
    odd = [x for x in s['fams'] if x in ('Other lines', 'Waves')]
    note = ''
    if fm == 'Chance':
        p = chance_parse(r); ok = p['fit'].startswith('fits'); note = '' if ok else p['fit']
        return fm, ok, note
    if fm == 'Precipitation (period)':
        ok = True   # the period is the picker's own Period dropdown (the id carries it only for the precip_p# charts)
    elif fm == 'Amount (expected, percentile)':
        ok = bool(re.search(r'percentile|_p\d+|pct\d+|median|expected|efi|extreme forecast|paintball', i + ' ' + lab)); note = '' if ok else 'statistic not read'
    elif fm == 'Ensemble (members, spread)':
        ok = bool(re.search(r'spread|spag|member|paintball|mean|_mem', i + ' ' + lab)); note = '' if ok else 'statistic not read'
        
    elif fm == 'Change':
        ok = bool(re.search(r'chng|change|trend|diff', i + ' ' + lab)); note = '' if ok else 'since not read'
    elif fm == 'Anomaly':
        ok = 'anom' in i + lab; note = '' if ok else 'anomaly not read'
    else:
        ok = s['field'] != '(lines only)' or bool(r['lines'])
        note = '' if ok else 'no fill, no lines'
    if odd and ok: ok = False; note = 'line family outside the list: ' + ', '.join(odd)
    return fm, ok, note
gfs_ids = {r['id'] for r in rows if r['src'] == 'GFS'}
mrows = []; exc = []
for src in sorted({r['src'] for r in rows}):
    rs = [r for r in rows if r['src'] == src]
    fams = collections.Counter(); bad = collections.Counter()
    for r in rs:
        fm, ok, note = axes(r); fams[fm] += 1
        if not ok: bad[note or 'other'] += 1; exc.append([src, r['id'], r['lab'], fm, note])
    ids = {r['id'] for r in rs}
    mrows.append([src, len(rs), len(rs) - sum(bad.values()), round(100 * (len(rs) - sum(bad.values())) / len(rs)), len(ids & gfs_ids), len(ids - gfs_ids),
                  *[fams[k] for k in FAMDEF], '; '.join(f'{n} x {k}' for k, n in bad.most_common(4))])
with open(os.path.join(here, 'models-check.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['model', 'chart entries', 'read by a family', 'percent read', 'ids shared with GFS', 'ids not in GFS'] + list(FAMDEF) + ['what did not read'])
    w.writerows(sorted(mrows, key=lambda x: (x[3], x[0])))
with open(os.path.join(here, 'models-check-exceptions.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['model', 'chart id', 'label', 'family', 'why it did not read']); w.writerows(exc)
tot = sum(x[1] for x in mrows); okn = sum(x[2] for x in mrows)
print(f'MODELS: {len(mrows)} models, {okn} of {tot} chart entries read by a family ({round(100*okn/tot)}%); models fully read {sum(1 for x in mrows if x[1]==x[2])}; under 90%: ' + ', '.join(f'{x[0]} {x[3]}%' for x in sorted(mrows, key=lambda x: x[3]) if x[3] < 90))

# --- the alias table: every distinct chart id read as a family and its settings (the id and the checks give each other)
byid2 = collections.defaultdict(list)
for r in rows: byid2[r['id']].append(r)
gsize = collections.Counter(key(i) for i in byid2)
refs_grid = {x['id']: x for x in csv.DictReader(open(os.path.join(here, 'spcrefs-grid.csv')))} if os.path.exists(os.path.join(here, 'spcrefs-grid.csv')) else {}
alias = []
for i, rs in sorted(byid2.items()):
    lead = collections.Counter((x['spec']['mode'], x['spec']['fams'], x['spec']['barbs']) for x in rs).most_common(1)[0][0]
    r = next(x for x in rs if (x['spec']['mode'], x['spec']['fams'], x['spec']['barbs']) == lead)
    fm, ok, note = axes(r); s = r['spec']
    p = chance_parse(r) if fm == 'Chance' else {}
    g = refs_grid.get(i) if {x['src'] for x in rs} == {'SPCREFS'} else None
    if g and g['fit'] == 'fits':
        fm, ok, note = 'Product grid (product, hour, type)', True, ''
        p = dict(product=g['product'], window=g['window'], limits=g['limit'], kind=g['type'])
    elif g and re.search(r'rh2m_s10m_prob', i):
        fm, ok, note = 'Chance', True, ''
    alias.append(dict(id=i, label=r['lab'], category=r['cat'], models=len(rs), family=fm if ok else 'One-off', reads=('yes' if ok else note), field=s['field'] if s['field'] != '(lines only)' else '', level=s['level'], mode=s['mode'],
                      chance=' | '.join(x for x in (p.get('product', ''), p.get('window', ''), p.get('limits', ''), p.get('kind', '')) if x), contours=', '.join(s['fams']), barbs='yes' if s['barbs'] else '', stream='yes' if s['stream'] else '', bands='yes' if s['ov'] else '',
                      group=key(i), group_size=gsize[key(i)], single='yes' if gsize[key(i)] == 1 else '', differs_by_model='yes' if len({(x['spec']['mode'], x['spec']['fams'], x['spec']['barbs']) for x in rs}) > 1 else ''))
cols = ['id', 'label', 'category', 'models', 'family', 'reads', 'field', 'level', 'mode', 'chance', 'contours', 'barbs', 'stream', 'bands', 'group', 'group_size', 'single', 'differs_by_model']
with open(os.path.join(here, 'alias-table.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, cols, extrasaction='ignore'); w.writeheader(); w.writerows(alias)
singles = [a for a in alias if a['family'] == 'One-off' or (a['single'] == 'yes' and a['family'] == 'Value (field at a level)')]   # alone in its id pattern and not folded into a family that has a number or a statistic to pick
print(f'ALIAS: {len(alias)} ids; family read {sum(1 for a in alias if a["family"] != "One-off")}; one-off {sum(1 for a in alias if a["family"] == "One-off")}; singles (group of 1) {sum(1 for a in alias if a["single"])}; singles or one-offs {len(singles)}')
# the singles: those that look alike (same mode, lines, barbs, bands) could be one preset with a field dropdown
clu = collections.defaultdict(list)
for a in singles: clu[(a['category'], a['mode'], a['contours'], a['barbs'], a['stream'], a['bands'])].append(a)
with open(os.path.join(here, 'singles.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['category', 'mode', 'contours', 'barbs', 'streamlines', 'bands', 'singles that look alike', 'one preset with a field dropdown could replace', 'models (most used)', 'examples (field)'])
    for k, v in sorted(clu.items(), key=lambda x: (x[0][0], -len(x[1]))):
        w.writerow([*k, len(v), 'yes' if len(v) > 1 else '', max(a['models'] for a in v), '; '.join(f"{a['id']} ({a['field'][:28]})" for a in v[:5])])
print(f'SINGLES: {len(singles)} ids in {len(clu)} look-alike groups; groups of 2 or more: {sum(1 for v in clu.values() if len(v) > 1)} holding {sum(len(v) for v in clu.values() if len(v) > 1)} ids; alone in their group: {sum(1 for v in clu.values() if len(v) == 1)}')
