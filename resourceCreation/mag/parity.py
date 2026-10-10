import json, re, collections
rows = json.load(open('recreate.json')); cap = json.load(open('captions.json'))
ours = {}
for l in open('/tmp/claude-1000/prod/layers_now.tsv'):
    p = l.rstrip('\n').split('\t')
    if len(p) >= 8: ours[(p[0], p[1])] = {'lines': p[4].lower(), 'barbs': p[5] == '1', 'stream': p[6] == '1', 'fill': p[7].lower()}
src = {'GFS-WAVE': 'GFS-WAVE', 'GEFS-WAVE': 'GEFS-WAVE', 'GFS': 'GFS', 'AIGFS': 'AIGFS', 'NBM': 'NBM', 'GEFS-MEAN-SPRD': 'GEFS', 'NAM': 'RRFS', 'NAM-HIRES': 'RRFS', 'HRRR': 'RRFS', 'RAP': 'RRFS', 'FIREWX': 'RRFS', 'HRW-FV3': 'RRFS', 'HRW-ARW': 'RRFS', 'HRW-ARW2': 'RRFS'}
alias = {('RAP', 'cape_cin'): 'sfc_cape_cin', ('NBM', '6hour_accu_snow'): 'snow_p06', ('NBM', 'total_accu_snow'): 'snow_ptot'}
def mag_layers(text):
    t = text.upper()
    barbs = bool(re.search(r'WND|WIND|HIND|HND|WINDS', t)) and not re.search(r'ISOTACH|SPEED|MAX |GUST', t)
    return {'MSLP lines': bool(re.search(r'EMSL|MMSL|MSLP|PMSL|\bMSL', t)), 'thickness lines': bool(re.search(r'THICK', t)), 'height lines': bool(re.search(r'H[BG]\w{0,3}T|HEIGHT|\bHT\b', t)), 'wind barbs': barbs}
def ours_layers(o):
    L = o['lines']
    return {'MSLP lines': 'pressure' in L or 'mslp' in L, 'thickness lines': 'hickness' in L, 'height lines': 'height' in L, 'wind barbs': o['barbs'] or o['stream']}
def clean(foot):
    s = foot.replace('»', ',').replace('¥', 'V').replace('|', ' ')
    return re.sub(r'^.*?[Vv]\s?[0-9O]{2,5}[A-Z0-9]?\s+', '', re.sub(r'\s+', ' ', s).strip(), count=1)
issues = collections.defaultdict(list); checked = 0
for r in rows:
    model, pid, label, status = r[0], r[1], r[2], r[3]
    if not status.startswith('recreated'): continue
    key = f'{model}/{pid}'
    if key not in cap: continue
    if model not in src: continue
    mine = ours.get((src[model], alias.get((model, pid), pid)))
    if not mine: continue
    checked += 1
    m = mag_layers(clean(cap[key][0])); o = ours_layers(mine)
    for k in m:
        if m[k] and not o[k]: issues[(model, pid)].append('missing ' + k)
        if o[k] and not m[k]: issues[(model, pid)].append('extra ' + k)
print(checked, 'recreated maps checked;', len(issues), 'differ in layers')
cnt = collections.Counter(i for v in issues.values() for i in v)
print(cnt.most_common())
by = collections.defaultdict(list)
for (m, p), v in sorted(issues.items()): by[tuple(v)].append(f'{m}/{p}')
for v, ps in sorted(by.items(), key=lambda x: -len(x[1])): print(len(ps), v, ps[:6])
json.dump({f'{m}/{p}': v for (m, p), v in issues.items()}, open('parity.json', 'w'), indent=1)
